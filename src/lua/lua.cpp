#include <errno.h>
#include <string.h>

#include "lua.h"

#define CLFACTORY_BEGIN_CODE "return function() "
#define CLFACTORY_BEGIN_SIZE (sizeof(CLFACTORY_BEGIN_CODE) - 1)

#define CLFACTORY_END_CODE "\nend"
#define CLFACTORY_END_SIZE (sizeof(CLFACTORY_END_CODE) - 1)

typedef struct {
    int         sent_begin;
    int         sent_end;
    int         extraline;
    FILE       *f;
    size_t      begin_code_len;
    size_t      end_code_len;
    size_t      rest_len;
    char       *begin_code;
    char       *end_code;
    char        buff[4096];
} lua_clfactory_file_ctx_t;



static DEF_PROC(lua);

DEF_LUA_PROC(get);
DEF_LUA_PROC(hget);
DEF_LUA_PROC(hgetall);
DEF_LUA_PROC(zscan);
DEF_LUA_PROC(resp);

Lua::Lua(lua_State *L){
	this->L = L;
    init_global(L);
}

Lua::~Lua(){
}

Lua* Lua::init(NetworkServer *serv){
	lua_State *L = luaL_newstate();
    if (L == NULL) {
		fprintf(stderr, "faile to new a lua vm");
		exit(1);
	}
    luaL_openlibs(L);
    
    ssdb_lua_set_server(L, (SSDBServer *)serv->data);

	Lua *vm = new Lua(L);
    
    serv->proc_map.set_proc("lua", "w", proc_lua);
    
	return vm;
}

void Lua::init_global(){
	//ngx_http_lua_init_registry
    lua_createtable(L, 0 /* narr */, 99 /* nrec */);    /* ssdb.* */
    
    init_proc_kv();
    init_response();
    
    lua_getglobal(L, "package"); /* ssdb package */
    lua_getfield(L, -1, "loaded"); /* ssdb package loaded */
    lua_pushvalue(L, -3); /* ssdb package loaded ssdb */
    lua_setfield(L, -2, "ssdb"); /* ssdb package loaded */
    lua_pop(L, 2);

	lua_setglobal(L, "ssdb");
    return;
}

void Lua::init_proc_kv(){
    lua_pushcfunction(L, lua_proc_get);
    lua_setfield(L, -2, "get");
    return;
}

void Lua::init_response(){
    lua_pushcfunction(L, lua_proc_resp);
    lua_setfield(L, -2, "echo");
    return;
}

void Lua::lua_cache_load_code(std::string cache_key){
	int          rc;
    u_char      *err;

    /*  get code cache table */
    lua_pushlightuserdata(L, &lua_code_cache_key);
    lua_rawget(L, LUA_REGISTRYINDEX);    /*  sp++ */

    if (!lua_istable(L, -1)) {
        return LUA_SSDB_ERR;
    }

    lua_getfield(L, -1, cache_key);    /*  sp++ */

    if (lua_isfunction(L, -1)) {
        /*  call closure factory to gen new closure */
        rc = lua_pcall(L, 0, 1, 0);
        if (rc == 0) {
            /*  remove cache table from stack, leave code chunk at
             *  top of stack */
            lua_remove(L, -2);   /*  sp-- */
            return LUA_SSDB_OK;
        }

        if (lua_isstring(L, -1)) {
            err = (u_char *) lua_tostring(L, -1);

        } else {
            err = (u_char *) "unknown error";
        }

        lua_pop(L, 2);
        return LUA_SSDB_ERR;
    }

    /*  remove cache table and value from stack */
    lua_pop(L, 2);                                /*  sp-=2 */

    return LUA_SSDB_DECLINED;
}

void Lua::lua_cache_store_code(std::string cache_key){
    int rc;

    /*  get code cache table */
    lua_pushlightuserdata(L, &lua_code_cache_key);
    lua_rawget(L, LUA_REGISTRYINDEX);

    if (!lua_istable(L, -1)) {
        return LUA_SSDB_ERR;
    }

    lua_pushvalue(L, -2); /* closure cache closure */
    /* ngx_http_lua_code_cache_key[key] = code */
    lua_setfield(L, -2, cache_key); /* closure cache */

    /*  remove cache table, leave closure factory at top of stack */
    lua_pop(L, 1); /* closure */

    /*  call closure factory to generate new closure */
    rc = lua_pcall(L, 0, 1, 0);
    if (rc != 0) {
        return LUA_SSDB_ERR;
    }

    return LUA_SSDB_OK;
}

void Lua::lua_clfactory_loadfile(std::string filename){
    int                         c, status, readstatus;
    ngx_flag_t                  sharp;

    lua_clfactory_file_ctx_t        lf;

    /* index of filename on the stack */
    int                         fname_index;

    sharp = 0;
    fname_index = lua_gettop(L) + 1;

    lf.extraline = 0;

    lf.begin_code.ptr = CLFACTORY_BEGIN_CODE; //return function() 
    lf.begin_code_len = CLFACTORY_BEGIN_SIZE;
    lf.end_code.ptr = CLFACTORY_END_CODE; // \nend
    lf.end_code_len = CLFACTORY_END_SIZE;

    lua_pushfstring(L, "@%s", filename);

    lf.f = fopen(filename, "r");

	// set the sent status
    lf.sent_begin = lf.sent_end = 0;

    status = lua_load(L, lua_clfactory_getF, &lf,
                      lua_tostring(L, -1));

    readstatus = ferror(lf.f);

    if (filename) {
        fclose(lf.f);  /* close file (even in case of errors) */
    }

    lua_remove(L, fname_index);

    return status;
}

void Lua::lua_clfactory_getF(lua_State *L, void *ud, size_t *size){
    char                        *buf;
    size_t                       num;

    lua_clfactory_file_ctx_t        *lf;

    lf = (lua_clfactory_file_ctx_t *) ud;

    if (lf->extraline) {
        lf->extraline = 0;
        *size = 1;
        return "\n";
    }

    if (lf->sent_begin == 0) {
        lf->sent_begin = 1;
        *size = lf->begin_code_len;
        buf = lf->begin_code.ptr;

        return buf;
    }

    num = fread(lf->buff, 1, sizeof(lf->buff), lf->f);

    if (num == 0) {
        if (lf->sent_end == 0) {
            lf->sent_end = 1;
            *size = lf->end_code_len;
            buf = lf->end_code.ptr;

            return buf;
        }

        *size = 0;
        return NULL;
    }

    *size = num;
    return lf->buff;
}

static int proc_lua(NetworkServer *net, Link *link, const Request &req, Response *resp){
    lua_State *L = net->hlua->L;
	ssdb_lua_set_resp(L, resp);
	std::string filepath = "hello.lua";
	std::string cache_key = "cache:" + filepath
    
    //TODO:store the code in L as cache
    int bRet = luaL_loadfile(L, "hello.lua");
    if(bRet)  
    {  
        resp->push_back("file_error");
        return 0;
    }

    bRet = lua_pcall(L, 0, 0, 0);  
    if(bRet)  
    {  
        resp->push_back("run_error");
        return 0;
    }

    //TODO:return the result from lua
	resp->push_back("ok");
	return 0;
}

/*
 * TODO:new co
 */
/*Lua* Lua::ngx_http_lua_new_thread(lua_State *L){
    
}*/
