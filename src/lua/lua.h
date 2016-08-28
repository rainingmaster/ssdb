#ifndef NET_LUA_H_
#define NET_LUA_H_

#include <vector>
#include <unistd.h>
#include <lua.hpp>

#include "../serv.h"
#include "../util/bytes.h"
#include "../net/server.h"

#define DEF_LUA_PROC(f) int lua_proc_##f(lua_State *L)
#define LUA_ERR "error"

#define lua_ssdb_server "_lua_ssdb_serv"

class Lua{
	private:
		void init_global(lua_State *L);
        void init_proc_kv(lua_State *L);
        void init_response(lua_State *L);
	public:
		Lua();
		~Lua();
		lua_State* L;
		static Lua* init(NetworkServer *serv);
};

static inline SSDBServer *
ssdb_lua_get_server(lua_State *L)
{
    SSDBServer *serv;

    lua_getglobal(L, lua_ssdb_server);
    serv = (SSDBServer *)lua_touserdata(L, -1);
    lua_pop(L, 1);

    return serv;
}

/*
 * 将整个 serv 作为userdata存在 L 中
 */
static inline void
ssdb_lua_set_server(lua_State *L, SSDBServer *serv)
{
    lua_pushlightuserdata(L, serv);
    lua_setglobal(L, lua_ssdb_server);
}

#endif
