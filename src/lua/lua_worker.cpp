/*
本方案需要在server->writer或server->reader中push进去job，供后面输出处理
*/
#include "lua_worker.h"
#include "../util/log.h"
#include "../include.h"

LuaWorker::LuaWorker(const std::string &name){
	this->name = name;
}

void LuaWorker::init(){
	log_debug("%s %d init", this->name.c_str(), this->id);
    hlua = Lua::init();
}

int LuaWorker::proc(LuaJob *job){
    lua_State *L = hlua->L;
	hlua->lua_execute_by_filepath(job->resp);
	hlua->lua_set_ssdb_serv(job->resp);

	hlua->lua_execute_by_filepath(job->filepath);
    //need to put the return val into server's result->push
    //like server worker->proc

	return 0;
}
