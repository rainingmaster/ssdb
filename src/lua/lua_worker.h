#ifndef SSDB_LUA_WORKER_H_
#define SSDB_LUA_WORKER_H_

#include <string>
#include "../util/thread.h"
#include "lua_handler.h"

class LuaHandler;
struct LuaJob{
	int result;
	std::string     filepath;
	NetworkServer  *serv;
	Link           *link;
	Request         req;
	Response       *resp;
	
	LuaJob()
    {
		result    = 0;
		serv      = NULL;
		link      = NULL;
		resp      = NULL;
	}
	~LuaJob()
    {
	}
};

class LuaWorker : public LWorkerPool<LuaWorker, LuaJob*>::Worker{
    private:
        LuaHandler*          lua;
    public:
        LuaWorker(const std::string &name);
        ~LuaWorker(){}
        void                 init();
        int                  proc(LuaJob *job);
};

typedef LWorkerPool<LuaWorker, LuaJob*> LuaWorkerPool;

#endif
