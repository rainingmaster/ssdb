#include <errno.h>

#include "../util/log.h"
#include "lua_handler.h"

#define CLFACTORY_BEGIN_CODE "return function() "
#define CLFACTORY_BEGIN_SIZE (sizeof(CLFACTORY_BEGIN_CODE) - 1)

#define CLFACTORY_END_CODE "\nend"
#define CLFACTORY_END_SIZE (sizeof(CLFACTORY_END_CODE) - 1)

static char lua_code_cache_key;

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

DEF_LUA_PROC(get);
DEF_LUA_PROC(hget);
DEF_LUA_PROC(hgetall);
DEF_LUA_PROC(zscan);
DEF_LUA_PROC(resp);

LuaHandler::LuaHandler(SSDBServer *serv)
{
    L = LuaHandler::init_lua_vm();
    ssdb_lua_set_server(L, serv);
}

LuaHandler::LuaHandler()
{
    L = LuaHandler::init_lua_vm();
}

LuaHandler::~LuaHandler()
{
	lua_close(L);
}

lua_State* LuaHandler::init_lua_vm()
{
	lua_State *l = luaL_newstate();
    if (l == NULL) {
		fprintf(stderr, "faile to new a lua vm");
		exit(1);
	}
    luaL_openlibs(l);

    lua_createtable(l, 0 /* narr */, 99 /* nrec */);    /* ssdb.* */
    
    init_proc_kv(l);
    init_response(l);
    
    lua_getglobal(l, "package"); /* ssdb package */
    lua_getfield(l, -1, "loaded"); /* ssdb package loaded */
    lua_pushvalue(l, -3); /* ssdb package loaded ssdb */
    lua_setfield(l, -2, "ssdb"); /* ssdb package loaded */
    lua_pop(l, 2);

	lua_setglobal(l, "ssdb");

	/*registry code cache table*/
	lua_pushlightuserdata(l, &lua_code_cache_key);
    lua_createtable(l, 0, 8 /* nrec */);
    lua_rawset(l, LUA_REGISTRYINDEX);

    return l;
}

void LuaHandler::init_proc_kv(lua_State *l)
{
    lua_pushcfunction(l, lua_proc_get);
    lua_setfield(l, -2, "get");
    lua_pushcfunction(l, lua_proc_hget);
    lua_setfield(l, -2, "hget");
    lua_pushcfunction(l, lua_proc_hgetall);
    lua_setfield(l, -2, "hgetall");
    lua_pushcfunction(l, lua_proc_zscan);
    lua_setfield(l, -2, "zscan");
    return;
}

void LuaHandler::init_response(lua_State *l){
    lua_pushcfunction(l, lua_proc_resp);
    lua_setfield(l, -2, "echo");
    return;
}

int LuaHandler::lua_cache_load_code(std::string *cache_key)
{
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

int LuaHandler::lua_cache_store_code(std::string *cache_key)
{
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

int LuaHandler::lua_clfactory_loadfile(std::string *filename)
{
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
    
    mutex.lock();

    lf.f = fopen(filename->c_str(), "r");

	// set the sent status
    lf.sent_begin = lf.sent_end = 0;

    status = lua_load(L, lua_clfactory_getF, &lf,
                      lua_tostring(L, -1));

    readstatus = ferror(lf.f);

    fclose(lf.f);
    
    mutex.unlock();

    lua_remove(L, fname_index);

    return status;
}

const char *
LuaHandler::lua_clfactory_getF(lua_State *L, void *ud, size_t *size)
{
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

int LuaHandler::lua_cache_loadfile(std::string *filename)
{
	cache_key = "cache:" + *filename;
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

int LuaHandler::lua_clear_file_cache(std::string *filename)
{
	cache_key = "cache:" + *filename;
    lua_pushlightuserdata(L, &lua_code_cache_key);
    lua_rawget(L, LUA_REGISTRYINDEX);    /*  sp++ */

    if (!lua_istable(L, -1)) {
        return LUA_SSDB_ERR;
    }

	lua_pushnil(L); /* del cache closure */
    lua_setfield(L, -2, cache_key.c_str());

    lua_pop(L, 1);
}

int LuaHandler::lua_set_ssdb_resp(Response *resp)
{
	ssdb_lua_set_resp(L, resp);
    return LUA_SSDB_OK;
}

int LuaHandler::lua_set_ssdb_serv(SSDBServer *serv)
{
	ssdb_lua_set_server(L, serv);
    return LUA_SSDB_OK;
}

int LuaHandler::lua_execute_by_filepath(std::string *filename)
{
	lua_cache_loadfile(filename);
    Response *resp = ssdb_lua_get_resp(L);

	if (lua_isfunction(L, -1)) {
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

