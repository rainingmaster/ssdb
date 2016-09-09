#ifndef SSDB_LUA_H_
#define SSDB_LUA_H_

#include <vector>
#include <unistd.h>
#include <string.h>
#include <lua.hpp>

#include "../serv.h"
#include "../util/bytes.h"
#include "../net/server.h"
#include "lua_thread.h"
#include "lua_worker.h"
#include "lua_handler.h"

#define DEF_LUA_PROC(f) int lua_proc_##f(lua_State *L)

#define LUA_SSDB_ERR       0
#define LUA_SSDB_OK        1
#define LUA_SSDB_DECLINED  2

struct LuaJob;
class LuaHandler;
class LuaWorker;
typedef LWorkerPool<LuaWorker, LuaJob*> LuaWorkerPool;

class Lua{
	private:
        LuaHandler*          lua;
        NetworkServer*       serv;
        LuaWorkerPool*       worker;
	public:
		Lua(NetworkServer* net);
		~Lua();
        int                  lua_execute_by_filepath(std::string *filepath);
        int                  lua_clear_file_cache(std::string *filepath);
        int                  lua_execute_by_thread(LuaJob *job);
        int                  lua_set_ssdb_resp(Response *resp);
        int                  lua_restart_thread();
};

#endif
