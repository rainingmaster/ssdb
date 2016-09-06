/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
#ifndef NET_WORKER_H_
#define NET_WORKER_H_

#include <string>
#include "../util/thread.h"
#include "proc.h"
#include "lua.h"

// WARN: pipe latency is about 20 us, it is really slow!
class LuaWorker : public WorkerPool<LuaWorker, LuaJob *>::Worker{
private:
    Lua *hlua;
public:
	LuaWorker(const std::string &name);
	~LuaWorker(){}
	void init();
	int proc(LuaJob *job);
};

struct LuaJob{
	int result;
	NetworkServer *serv;
    std::string lua_file;
	
	const Request *req;
	Response resp;
	
	LuaJob(){
		result = 0;
		serv = NULL;
	}
	~LuaJob(){
	}
};

typedef WorkerPool<LuaWorker, LuaJob *> LuaWorkerPool;

#endif
