#include <errno.h>

#include "lua.h"
#include "../util/log.h"

static DEF_PROC(lua);
static DEF_PROC(lua_clear);
static DEF_PROC(lua_thread);

Lua::Lua(NetworkServer* net)
{
    net->proc_map.set_proc("lua", "w", proc_lua);
    net->proc_map.set_proc("lua_clear", "w", proc_lua_clear);
    net->proc_map.set_proc("lua_clear", "w", proc_lua_thread);

	lua = new LuaHandler((SSDBServer *)serv->data);
    serv = net;

    worker = new LuaWorkerPool("lua thread");
	worker->start(10);
}

Lua::~Lua()
{
	delete lua;
	worker->stop();
    delete worker;
}

int Lua::lua_execute_by_filepath(std::string *filepath)
{
	return lua->lua_execute_by_filepath(filepath);
}

int Lua::lua_execute_by_thread(LuaJob *job)
{
	worker->push(job);
    return LUA_SSDB_OK;
}

int Lua::lua_clear_file_cache(std::string *filepath)
{
	return lua->lua_clear_file_cache(filepath);
}

int Lua::lua_set_ssdb_resp(Response *resp)
{
	return lua->lua_set_ssdb_resp(resp);
}

static int proc_lua(NetworkServer *net, Link *link, const Request &req, Response *resp){
	Lua *hlua = net->hlua;

	hlua->lua_set_ssdb_resp(resp);

	std::string filepath (req[1].data(), req[1].size());
	hlua->lua_execute_by_filepath(&filepath);

	return 0;
}

static int proc_lua_clear(NetworkServer *net, Link *link, const Request &req, Response *resp){
	Lua *hlua = net->hlua;
    
	std::string filepath (req[1].data(), req[1].size());
	hlua->lua_clear_file_cache(&filepath);

	resp->push_back("ok");
	return 0;
}

static int proc_lua_thread(NetworkServer *net, Link *link, const Request &req, Response *resp){
	Lua *hlua = net->hlua;

	std::string filepath (req[1].data(), req[1].size());

    LuaJob *job = new LuaJob();
    job->serv     = net;
    job->link     = link;
    job->resp     = resp;
    job->filepath = &filepath;

	hlua->lua_execute_by_thread(job);

	//resp->push_back("ok");
	return 0;
}
