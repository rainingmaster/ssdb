#include <errno.h>
#include <string.h>

#include "lua.h"
#include "../util/log.h"

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
    std::string begin_code;
    std::string end_code;
    char        buff[4096];
} lua_clfactory_file_ctx_t;



static DEF_PROC(lua);
static DEF_PROC(lua_thread);
static DEF_PROC(lua_clear);

DEF_LUA_PROC(get);
DEF_LUA_PROC(hget);
DEF_LUA_PROC(hgetall);
DEF_LUA_PROC(zscan);
DEF_LUA_PROC(resp);

Lua::Lua(lua_State *L){
	this->L = L;
    init_global();
}

Lua::~Lua(){
	lua_close(L);
}

/* init with server, use in function server */
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
    serv->proc_map.set_proc("lua_thread", "rt", proc_lua_thread);
    serv->proc_map.set_proc("lua_clear", "w", proc_lua_clear);
    
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

	/*registry code cache table*/
	lua_pushlightuserdata(L, &lua_code_cache_key);
    lua_createtable(L, 0, 8 /* nrec */);
    lua_rawset(L, LUA_REGISTRYINDEX);

    return;
}

void Lua::init_proc_kv(){
    lua_pushcfunction(L, lua_proc_get);
    lua_setfield(L, -2, "get");
    lua_pushcfunction(L, lua_proc_hget);
    lua_setfield(L, -2, "hget");
    lua_pushcfunction(L, lua_proc_hgetall);
    lua_setfield(L, -2, "hgetall");
    lua_pushcfunction(L, lua_proc_zscan);
    lua_setfield(L, -2, "zscan");
    return;
}

void Lua::init_response(){
    lua_pushcfunction(L, lua_proc_resp);
    lua_setfield(L, -2, "echo");
    return;
}

int Lua::lua_cache_load_code(std::string *cache_key){
	int          rc;
    u_char      *err;

    /*  get code cache table */
    lua_pushlightuserdata(L, &lua_code_cache_key);
    lua_rawget(L, LUA_REGISTRYINDEX);    /*  sp++ */

    if (!lua_istable(L, -1)) {
        return LUA_SSDB_ERR;
    }

    lua_getfield(L, -1, cache_key->c_str());    /*  sp++ */

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

int Lua::lua_cache_store_code(std::string *cache_key){
    int rc;

    /*  get code cache table */
    lua_pushlightuserdata(L, &lua_code_cache_key);
    lua_rawget(L, LUA_REGISTRYINDEX);

    if (!lua_istable(L, -1)) {
        return LUA_SSDB_ERR;
    }

    lua_pushvalue(L, -2); /* closure cache closure */
    /* lua_code_cache_key[key] = code */
    lua_setfield(L, -2, cache_key->c_str()); /* closure cache */

    /*  remove cache table, leave closure factory at top of stack */
    lua_pop(L, 1); /* closure */

    /*  call closure factory to generate new closure */
    rc = lua_pcall(L, 0, 1, 0);
    if (rc != 0) {
        return LUA_SSDB_ERR;
    }

    return LUA_SSDB_OK;
}

int Lua::lua_clfactory_loadfile(std::string *filename){
    int                             status, readstatus;

    lua_clfactory_file_ctx_t        lf;

    /* index of filename on the stack */
    int                         fname_index;

    fname_index = lua_gettop(L) + 1;

    lf.extraline = 0;

    lf.begin_code = CLFACTORY_BEGIN_CODE; //return function() 
    lf.begin_code_len = CLFACTORY_BEGIN_SIZE;
    lf.end_code = CLFACTORY_END_CODE; // \nend
    lf.end_code_len = CLFACTORY_END_SIZE;

    lua_pushfstring(L, "@%s", filename->c_str());

    lf.f = fopen(filename->c_str(), "r");

	// set the sent status
    lf.sent_begin = lf.sent_end = 0;

    status = lua_load(L, lua_clfactory_getF, &lf,
                      lua_tostring(L, -1));

    readstatus = ferror(lf.f);

    fclose(lf.f);

    lua_remove(L, fname_index);

    return status;
}

const char *
Lua::lua_clfactory_getF(lua_State *L, void *ud, size_t *size){
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

        return lf->begin_code.c_str();
    }

    num = fread(lf->buff, 1, sizeof(lf->buff), lf->f);

    if (num == 0) {
        if (lf->sent_end == 0) {
            lf->sent_end = 1;
            *size = lf->end_code_len;

            return lf->end_code.c_str();
        }

        *size = 0;
        return NULL;
    }

    *size = num;
    return lf->buff;
}

int Lua::lua_cache_loadfile(std::string *filename) {
	std::string cache_key = "cache:" + (*filename);
    const char      *err = NULL;

    int n = lua_gettop(L);

	int rc = lua_cache_load_code(&cache_key);
    if (rc == LUA_SSDB_OK) {
        /*  code chunk loaded from cache, sp++ */
        return LUA_SSDB_OK;
    }

    if (rc == LUA_SSDB_ERR) {
        return LUA_SSDB_ERR;
    }

    /*  load closure factory of script file to the top of lua stack, sp++ */
    rc = lua_clfactory_loadfile(filename);

    if (rc != 0) {
        switch (rc) {
        case LUA_ERRMEM:
            err = "memory allocation error";
            break;

        case LUA_ERRFILE:
            err = "not found";

        default:
            if (lua_isstring(L, -1)) {
                err = lua_tostring(L, -1);

            } else {
                err = "unknown error";
            }
        }

        goto error;
    }

    /*  store closure factory and gen new closure at the top of lua stack
     *  to code cache */
    rc = lua_cache_store_code(&cache_key);

    return LUA_SSDB_OK;


error:
    lua_settop(L, n);
    return LUA_SSDB_ERR;
}

int Lua::lua_clear_file_cache(std::string *filename){
	std::string cache_key = "cache:" + (*filename);
    lua_pushlightuserdata(L, &lua_code_cache_key);
    lua_rawget(L, LUA_REGISTRYINDEX);    /*  sp++ */

    if (!lua_istable(L, -1)) {
        return LUA_SSDB_ERR;
    }

	lua_pushnil(L); /* del cache closure */
    lua_setfield(L, -2, cache_key.c_str());

    lua_pop(L, 1);
}

lua_State*
Lua::lua_new_thread(){
    int              base;
    lua_State       *co;

    base = lua_gettop(L);

    co = lua_newthread(L);

    lua_settop(L, base);
    return co;
}

int Lua::lua_execute_by_filename(std::string *filename, Response *resp){
	lua_cache_loadfile(filename);

	if (lua_isfunction(L, -1)) {
        ssdb_lua_set_resp(L, resp);
		int bRet = lua_pcall(L, 0, 0, 0);
		if(bRet) 
		{
    		//TODO:echo the error to the log file instand of return data
			std::string err = lua_tostring(L, -1);
 			log_error("lua code error: %s", err.data());
			lua_pop(L, 1);
			resp->push_back("run_error");
			return LUA_SSDB_OK;
		}
	}
    return LUA_SSDB_ERR;
}

int Lua::lua_execute_by_thread(std::string *filename, Response *resp){
    mutex.lock();
    lua_cache_loadfile(filename);

	if (lua_isfunction(L, -1)) {
        lua_State *co = lua_new_thread();

        /* inherit the L's global table to be __index */
        lua_pushvalue(co, LUA_GLOBALSINDEX);
        lua_setfield(co, -1, "__index");
        
        /*  set closure's env table to new coroutine's globals table, to store the resp */
        lua_createtable(co, 0, 1);
        lua_replace(co, LUA_GLOBALSINDEX);
        
        /* set the global table to env */
        lua_pushvalue(co, LUA_GLOBALSINDEX);
        lua_setfenv(co, -1);

        //copy the code to co
        lua_xmove(L, co, 1);
        
        mutex.unlock();

        /* add the response to the new gobal table */
        ssdb_lua_set_resp(co, resp);
        int bRet = lua_resume(co, 0);

		if(bRet) 
		{
    		//TODO:echo the error to the log file instand of return data
			std::string err = lua_tostring(co, -1);
 			log_error("lua code error: %s", err.data());
			lua_pop(co, 1);
			resp->push_back("run_error");
			return LUA_SSDB_ERR;
		}
        return LUA_SSDB_OK;
	}else{
        mutex.unlock();
    }
    return LUA_SSDB_ERR;
}

//TODO:a new method in lua for read through thread
static int proc_lua(NetworkServer *net, Link *link, const Request &req, Response *resp){
	Lua *hlua = net->hlua;

	std::string filename = req[1].String();
	hlua->lua_execute_by_filename(&filename, resp);

	return 0;
}

static int proc_lua_thread(NetworkServer *net, Link *link, const Request &req, Response *resp){
    //use a lua_newthread to excute the file, and use the ssdb's thread worker
	Lua *hlua = net->hlua;

	std::string filename = req[1].String();
	hlua->lua_execute_by_thread(&filename, resp);

	return 0;
}

static int proc_lua_clear(NetworkServer *net, Link *link, const Request &req, Response *resp){
	Lua *hlua = net->hlua;
    
	std::string filename = req[1].String();
	hlua->lua_clear_file_cache(&filename);

	resp->push_back("ok");
	return 0;
}

/*
 * TODO:new co
 */
/*Lua* Lua::ngx_http_lua_new_thread(lua_State *L){
    
}*/
