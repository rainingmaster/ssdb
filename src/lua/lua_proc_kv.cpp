/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
/* kv */
#include "lua.h"
#include "../net/proc.h"
#include "../net/server.h"

int lua_proc_get(lua_State *L){
	CHECK_LUA_NUM_PARAMS(1);
	SSDBServer *serv = ssdb_lua_get_server(L);
    
    Bytes req = lua_tostring(L, 1);

	std::string val;
	serv->ssdb->get(req, &val);
	lua_pushlstring(L, val.data(), val.size());
    return 1;
}

int lua_proc_hget(lua_State *L){
	CHECK_LUA_NUM_PARAMS(2);
	SSDBServer *serv = ssdb_lua_get_server(L);
    Bytes hash = lua_tostring(L, 1);
    Bytes key = lua_tostring(L, 2);

	std::string val;
	int ret = serv->ssdb->hget(hash, key, &val);
	lua_pushlstring(L, val.data(), val.size());
	return 0;
}

int lua_proc_hgetall(lua_State *L){
	CHECK_LUA_NUM_PARAMS(1);
	SSDBServer *serv = ssdb_lua_get_server(L);

    Bytes req = lua_tostring(L, 1);
	//return a table, use the hash key and value
	//TODO:use a ensure size of the table
	lua_createtable(L, 0, 2000);

	HIterator *it = serv->ssdb->hscan(req, "", "", 2000000000);
	while(it->next()){
	    lua_pushlstring(L, it->key.data(), it->key.size());
	    lua_pushlstring(L, it->val.data(), it->val.size());
	    lua_rawset(L, -3);
	}
	delete it;
	return 1;
}

int lua_proc_zscan(lua_State *L){
	CHECK_LUA_NUM_PARAMS(5);
	SSDBServer *serv = ssdb_lua_get_server(L);

	uint64_t limit = lua_checkint(L, 5);
	uint64_t offset = 0;
	if(lua_gettop(L) > 6){
		offset = limit;
		limit = offset + lua_checkint(L, 6);
	}
    Bytes req1 = lua_tostring(L, 1);
    Bytes req2 = lua_tostring(L, 2);
    Bytes req3 = lua_tostring(L, 3);
    Bytes req4 = lua_tostring(L, 4);
	
	ZIterator *it = serv->ssdb->zscan(req1, req2, req3, req4, limit);
	if(offset > 0){
		it->skip(offset);
	}
	//TODO:use a ensure size of the table
	lua_createtable(L, 0, 2000);
	while(it->next()){
	    lua_pushlstring(L, it->key.data(), it->key.size());
	    lua_pushlstring(L, it->score.data(), it->score.size());
	    lua_rawset(L, -3);
	}
	delete it;
	return 0;
}

