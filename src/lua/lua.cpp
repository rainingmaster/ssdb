#include <errno.h>
#include <string.h>

#include "lua.h"

static DEF_PROC(lua);

static DEF_LUA_PROC(get);
static DEF_LUA_PROC(resp);

Lua::Lua(){
    init_global(L);
}

Lua::~Lua(){
}

Lua* Lua::init(NetworkServer *serv){
	lua_State *L = luaL_newstate();
    if (L == NULL) {
		fprintf(stderr, "faile to new a lua vm");
		exit(1);
	}
    luaL_openlibs(L);
    
    ssdb_lua_set_server(L, (SSDBServer *)serv->data);

	Lua *vm = new Lua();
	vm->L = L;
    
    serv->proc_map.set_proc("lua", "w", proc_lua);
    
	return vm;
}

void Lua::init_global(lua_State *L){
	//ngx_http_lua_init_registry
    lua_createtable(L, 0 /* narr */, 99 /* nrec */);    /* ssdb.* */
    
    init_proc_kv(L);
    init_response(L);
    
    lua_getglobal(L, "package"); /* ssdb package */
    lua_getfield(L, -1, "loaded"); /* ssdb package loaded */
    lua_pushvalue(L, -3); /* ssdb package loaded ssdb */
    lua_setfield(L, -2, "ssdb"); /* ssdb package loaded */
    lua_pop(L, 2);
    return;
}

void Lua::init_proc_kv(lua_State *L){
    lua_pushcfunction(L, lua_proc_get);
    lua_setfield(L, -2, "get");
    return;
}

void Lua::init_response(lua_State *L){
    lua_pushcfunction(L, lua_proc_resp);
    lua_setfield(L, -2, "retrun");
    return;
}

static int proc_lua(NetworkServer *net, Link *link, const Request &req, Response *resp){
    lua_State *L = net->hlua->L;
    
    //TODO:需要放入 L 缓存
    int bRet = luaL_loadfile(L, "hello.lua");
    if(bRet)  
    {  
        resp->push_back("file_error");
        return 0;
    }

    bRet = lua_pcall(L, 0, 0, 0);  
    if(bRet)  
    {  
        resp->push_back("run_error");
        return 0;
    }

    //获取 lua 返回值
	resp->push_back("ok");
	return 0;
}

/*
 * TODO:new co
 */
/*Lua* Lua::ngx_http_lua_new_thread(lua_State *L){
    
}*/
