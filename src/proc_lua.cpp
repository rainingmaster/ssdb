/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
/* kv */
#include "serv.h"
#include "net/proc.h"
#include "net/server.h"


int proc_lua(NetworkServer *net, Link *link, const Request &req, Response *resp){
    //1.创建Lua状态  
    lua_State *L = luaL_newstate();  
    if (L == NULL)  
    {  
        resp->push_back("client_error");
        return 0;
    }  

    //2.加载Lua文件 
    int bRet = luaL_loadfile(L,"hello.lua");  
    if(bRet)  
    {  
        resp->push_back("client_error");
        return 0;
    }  
   
    //3.运行Lua文件  
    bRet = lua_pcall(L, 0, 0, 0);  
    if(bRet)  
    {  
        resp->push_back("client_error");
        return 0;
    }
   
    //4.读取table  
    lua_getglobal(L, "tbl");
    lua_getfield(L, -1, "name");  
    std::string str = lua_tostring(L, -1);
    Bytes* s = new Bytes(str);
	SSDBServer *serv = (SSDBServer *)net->data;

	std::string val;
	int ret = serv->ssdb->get(*s, &val);
	resp->reply_get(ret, &val);
	return 0;
}
