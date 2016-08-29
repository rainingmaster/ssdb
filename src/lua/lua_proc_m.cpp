/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
/* kv */
#include "lua.h"
#include "../net/proc.h"

int lua_proc_resp(lua_State *L){
    std::string ret = luaL_checkstring(L, 1);

	Response *resp = ssdb_lua_get_resp(L);
	resp->push_back(ret);
    return 1;
}