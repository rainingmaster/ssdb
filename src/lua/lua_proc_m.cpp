/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
/* kv */
#include "lua.h"
#include "../net/proc.h"

int lua_proc_resp(lua_State *L){
    Bytes req = luaL_checkstring(L, 1);
    if(req == NULL){
        lua_pushliteral(L, LUA_ERR);
        return 1;
    }
	lua_pushlstring(L, req.data(), req.size());
    return 1;
}