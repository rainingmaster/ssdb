#include <errno.h>

#include "lua.h"
#include "../util/log.h"

static DEF_PROC(lua);
static DEF_PROC(lua_clear);
static DEF_PROC(lua_thread);
static DEF_PROC(lua_thread_clear);
static int lua_thread_num = 10;

Lua::Lua(NetworkServer* net)
{
    net->proc_map.set_proc("lua", "w", proc_lua);
    net->proc_map.set_proc("lua_clear", "w", proc_lua_clear);
    net->proc_map.set_proc("lua_thread", "b", proc_lua_thread);
    net->proc_map.set_proc("lua_thread_clear", "r", proc_lua_thread_clear);

	lua = new LuaHandler((SSDBServer *)net->data);
    serv = net;

    worker = new LuaWorkerPool("lua thread");
	worker->start(lua_thread_num);
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

int Lua::lua_restart_thread()
{
    /* restart the worker to restart the lua VM */
	worker->stop();
    sleep((unsigned int)0.2);
	worker->start(lua_thread_num);

	return LUA_SSDB_OK;
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
    job->filepath = filepath;

	hlua->lua_execute_by_thread(job);

	return PROC_THREAD;
}

static int proc_lua_thread_clear(NetworkServer *net, Link *link, const Request &req, Response *resp){
	Lua *hlua = net->hlua;
    hlua->lua_restart_thread();

	resp->push_back("ok");
	return 0;
}
