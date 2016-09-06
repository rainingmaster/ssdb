#ifndef NET_LUA_H_
#define NET_LUA_H_

#include <vector>
#include <unistd.h>
#include <lua.hpp>

#include "../serv.h"
#include "../util/bytes.h"
#include "../net/server.h"
#include "../util/thread.h"

#define DEF_LUA_PROC(f) int lua_proc_##f(lua_State *L)

#define LUA_SSDB_ERR       0
#define LUA_SSDB_OK        1
#define LUA_SSDB_DECLINED  2

#define CHECK_LUA_NUM_PARAMS(n) if (lua_gettop(L) < n) { \
        return luaL_error(L, "wrong number of arguments"); \
    }

#define lua_ssdb_server   "_lua_ssdb_serv"
#define lua_ssdb_response "_lua_ssdb_resp"

class SSDBServer;

class Lua{
	private:
		lua_State*            L;
        char                  lua_code_cache_key;
		void                  init_global();
        void                  init_proc_kv();
        void                  init_response();
        int                   lua_cache_load_code(std::string *cache_key);
        int                   lua_cache_store_code(std::string *cache_key);
        int                   lua_clfactory_loadfile(std::string *filename);
        int                   lua_cache_loadfile(std::string *filename);
        lua_State*            lua_new_thread();
        static const char*    lua_clfactory_getF(lua_State *L, void *ud, size_t *size);
        Mutex                 mutex;
	public:
		Lua(lua_State *L);
		~Lua();
		static                Lua* init(NetworkServer *serv);
        int                   lua_clear_file_cache(std::string *filename);
		int                   lua_execute_by_filename(std::string *filename, Response *resp);
        int                   lua_execute_by_thread(std::string *filename, Response *resp);
};

static inline SSDBServer*
ssdb_lua_get_server(lua_State *L)
{
    SSDBServer *serv;

    lua_pushliteral(L, lua_ssdb_server);
    lua_rawget(L, LUA_REGISTRYINDEX);
    serv = (SSDBServer *)lua_touserdata(L, -1);
    lua_pop(L, 1);

    return serv;
}

/*
 * store the serv into L as userdata
 */
static inline void
ssdb_lua_set_server(lua_State *L, SSDBServer *serv)
{
    lua_pushliteral(L, lua_ssdb_server);
    lua_pushlightuserdata(L, serv);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

static inline Response*
ssdb_lua_get_resp(lua_State *L)
{
    Response *resp;

    lua_getglobal(L, lua_ssdb_response);
    resp = (Response *)lua_touserdata(L, -1);
    lua_pop(L, 1);

    return resp;
}

/*
 * store the resp into L as userdata
 */
static inline void
ssdb_lua_set_resp(lua_State *L, Response *resp)
{
    lua_pushlightuserdata(L, resp);
    lua_setglobal(L, lua_ssdb_response);
}

/*
 * push the return value into lua stack
 */
static inline void
ssdb_lua_push_str(lua_State *L, int status, std::string val)
{
    if(status == -1){ //error
        lua_pushliteral(L, "error");
        lua_pushnil(L);
    } else if(status == 0){ //not_found
        lua_pushliteral(L, "not_found");
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, val.data(), val.size());
    }
}

#endif
