/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
#ifndef SSDB_LUA_WORKER_H_
#define SSDB_LUA_WORKER_H_

#include <string>
#include "../util/thread.h"
#include "lua_handler.h"

class LuaHandler;
struct LuaJob{
	int result;
	std::string     *filepath;
	NetworkServer   *serv;
	Link            *link;
	Request         *req;
	Response        *resp;
	
	LuaJob()
    {
		result    = 0;
		filepath  = NULL;
		serv      = NULL;
		link      = NULL;
		req       = NULL;
		resp      = NULL;
	}
	~LuaJob()
    {
	}
};

/*class LuaWorker : public WorkerPool<LuaWorker, LuaJob*>::Worker{
    private:
        LuaHandler*          lua;
    public:
        LuaWorker(const std::string &name);
        ~LuaWorker(){}
        void                 init();
        void                 destroy();
        int                  proc(LuaJob *job);
};*/

class LuaWorker : public WorkerPool<LuaWorker, LuaJob>::Worker{
public:
	LuaWorker(const std::string &name);
	~LuaWorker(){}
	void init();
	int proc(LuaJob *job);
};

typedef WorkerPool<LuaWorker, LuaJob*> LuaWorkerPool;

#endif
