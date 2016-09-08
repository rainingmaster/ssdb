/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
/* kv */
#include "lua_handler.h"
#include "../net/proc.h"

int lua_proc_resp(lua_State *L){
    std::string ret = luaL_checkstring(L, 1);

	Response *resp = ssdb_lua_get_resp(L);
	resp->push_back(ret);
    return 1;
}

int lua_proc_log(lua_State *L){
    const char *msg;
    int level = luaL_checkint(L, 1);
    if (level < Logger::LEVEL_MIN || level > Logger::LEVEL_MAX) {
        msg = lua_pushfstring(L, "bad log level: %d", level);
        return luaL_argerror(L, 1, msg);
    }

    std::string c = luaL_checkstring(L, 2);
    c = "%s(%d): [lua] " + c;
    log_write(level, c.c_str(), __FILE__, __LINE__);
    return 1;
}