#include <errno.h>
#include <string.h>

#include "lua.h"

static DEF_PROC(lua);

DEF_LUA_PROC(get);
DEF_LUA_PROC(hget);
DEF_LUA_PROC(hgetall);
DEF_LUA_PROC(zscan);
DEF_LUA_PROC(resp);

Lua::Lua(lua_State *L){
	this->L = L;
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

	Lua *vm = new Lua(L);
    
    serv->proc_map.set_proc("lua", "w", proc_lua);
    
	return vm;
}

void Lua::init_global(){
	//ngx_http_lua_init_registry
    lua_createtable(L, 0 /* narr */, 99 /* nrec */);    /* ssdb.* */
    
    init_proc_kv();
    init_response();
    
    lua_getglobal(L, "package"); /* ssdb package */
    lua_getfield(L, -1, "loaded"); /* ssdb package loaded */
    lua_pushvalue(L, -3); /* ssdb package loaded ssdb */
    lua_setfield(L, -2, "ssdb"); /* ssdb package loaded */
    lua_pop(L, 2);

	lua_setglobal(L, "ssdb");
    return;
}

void Lua::init_proc_kv(){
    lua_pushcfunction(L, lua_proc_get);
    lua_setfield(L, -2, "get");
    return;
}

void Lua::init_response(){
    lua_pushcfunction(L, lua_proc_resp);
    lua_setfield(L, -2, "echo");
    return;
}

void Lua::lua_cache_load_code(std::string cache_key){
	int          rc;
    u_char      *err;

    /*  get code cache table */
    /* 获取 key 为 ngx_http_lua_code_cache_key 的表 */
    lua_pushlightuserdata(L, &lua_code_cache_key);
    lua_rawget(L, LUA_REGISTRYINDEX);    /*  sp++ */

    if (!lua_istable(L, -1)) { // lua_code_cache_key 的表无效
        return LUA_SSDB_ERR;
    }

    //查找对应代码并推到栈顶
    lua_getfield(L, -1, cache_key);    /*  sp++ */

    //栈顶内容是function,即设置过代码chunk的缓存
    if (lua_isfunction(L, -1)) {
        /*  call closure factory to gen new closure */
        /* 栈顶的function为:
         * return function() ... 
         * \nend
         * 当使用pcall，将上述函数放入栈顶
         */
        rc = lua_pcall(L, 0, 1, 0);
        if (rc == 0) {
            /*  remove cache table from stack, leave code chunk at
             *  top of stack */
            //将之前的 lua_pushlightuserdata 和 lua_getfield 删除
            lua_remove(L, -2);   /*  sp-- */
            return LUA_SSDB_OK;
        }

        if (lua_isstring(L, -1)) { //错误信息
            err = (u_char *) lua_tostring(L, -1);

        } else {
            err = (u_char *) "unknown error";
        }

        lua_pop(L, 2);
        return LUA_SSDB_ERR;
    }

    /*  remove cache table and value from stack */
    //将之前的 lua_pushlightuserdata 和 lua_getfield 删除
    lua_pop(L, 2);                                /*  sp-=2 */

    return LUA_SSDB_DECLINED;
}

void Lua::lua_cache_store_code(std::string cache_key){
    int rc;

    /*  get code cache table */
    lua_pushlightuserdata(L, &lua_code_cache_key);
    lua_rawget(L, LUA_REGISTRYINDEX);

    if (!lua_istable(L, -1)) {
        return LUA_SSDB_ERR;
    }

    /* 栈顶值 ngx_http_lua_code_cache_key 表，为将栈顶第2个值，即lua代码，复制压栈 */
    lua_pushvalue(L, -2); /* closure cache closure */
    /* ngx_http_lua_code_cache_key[key] = code */
    lua_setfield(L, -2, cache_key); /* closure cache */

    /*  remove cache table, leave closure factory at top of stack */
    lua_pop(L, 1); /* closure */

    /*  call closure factory to generate new closure */
    rc = lua_pcall(L, 0, 1, 0);
    if (rc != 0) {
        return LUA_SSDB_ERR;
    }

    return LUA_SSDB_OK;
}

void Lua::lua_clfactory_loadfile(std::string filename){
    int                         c, status, readstatus;
    ngx_flag_t                  sharp;

    ngx_http_lua_clfactory_file_ctx_t        lf;

    /* index of filename on the stack */
    int                         fname_index;

    sharp = 0;
    fname_index = lua_gettop(L) + 1;

    lf.extraline = 0;
    lf.file_type = NGX_LUA_TEXT_FILE;

    lf.begin_code.ptr = CLFACTORY_BEGIN_CODE; //return function() 
    lf.begin_code_len = CLFACTORY_BEGIN_SIZE;
    lf.end_code.ptr = CLFACTORY_END_CODE; // \nend
    lf.end_code_len = CLFACTORY_END_SIZE;

    lua_pushfstring(L, "@%s", filename);

    lf.f = fopen(filename, "r");
    if (lf.f == NULL) {
        return ngx_http_lua_clfactory_errfile(L, "open", fname_index);
    }

    c = getc(lf.f);

    if (c == '#') {  /* Unix exec. file? */ //首字母为 #
        lf.extraline = 1;

        while ((c = getc(lf.f)) != EOF && c != '\n') {
            /* skip first line */
        }

        if (c == '\n') {
            c = getc(lf.f);
        }

        sharp = 1;
    }

    if (c == LUA_SIGNATURE[0] && filename) {  /* binary file? */
        lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */

        if (lf.f == NULL) {
            return ngx_http_lua_clfactory_errfile(L, "reopen", fname_index);
        }

        /* check whether lib jit exists */
        luaL_findtable(L, LUA_REGISTRYINDEX, "_LOADED", 1);
        lua_getfield(L, -1, "jit");  /* get _LOADED["jit"] */

        if (lua_istable(L, -1)) {
            lf.file_type = NGX_LUA_BT_LJ;

        } else {
            lf.file_type = NGX_LUA_BT_LUA;
        }

        lua_pop(L, 2);

        /*
         * Loading bytecode with an extra header is disabled for security
         * reasons. This may circumvent the usual check for bytecode vs.
         * Lua code by looking at the first char. Since this is a potential
         * security violation no attempt is made to echo the chunkname either.
         */
        if (lf.file_type == NGX_LUA_BT_LJ && sharp) {

            if (filename) {
                fclose(lf.f);  /* close file (even in case of errors) */
            }

            filename = lua_tostring(L, fname_index) + 1;
            lua_pushfstring(L, "bad byte-code header in %s", filename);
            lua_remove(L, fname_index);

            return LUA_ERRFILE;
        }

        while ((c = getc(lf.f)) != EOF && c != LUA_SIGNATURE[0]) {
            /* skip eventual `#!...' */
        }

        status = ngx_http_lua_clfactory_bytecode_prepare(L, &lf, fname_index);

        if (status != 0) {
            return status;
        }

        lf.extraline = 0;
    }

    if (lf.file_type == NGX_LUA_TEXT_FILE) { //文本型文件
        ungetc(c, lf.f); //一个（或多个）字符退回到输入流
    }

    lf.sent_begin = lf.sent_end = 0;
    //使用 ngx_http_lua_clfactory_getF 解析 lf中的内容，形成代码 chunk，至于栈顶
    status = lua_load(L, ngx_http_lua_clfactory_getF, &lf,
                      lua_tostring(L, -1));

    readstatus = ferror(lf.f);

    if (filename) {
        fclose(lf.f);  /* close file (even in case of errors) */
    }

    if (readstatus) {
        lua_settop(L, fname_index);  /* ignore results from `lua_load' */
        return ngx_http_lua_clfactory_errfile(L, "read", fname_index);
    }

    lua_remove(L, fname_index);

    return status;
}

static int proc_lua(NetworkServer *net, Link *link, const Request &req, Response *resp){
    lua_State *L = net->hlua->L;
	ssdb_lua_set_resp(L, resp);
	std::string filepath = "hello.lua";
	std::string cache_key = "cache:" + filepath
    
    //TODO:store the code in L as cache
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

    //TODO:return the result from lua
	resp->push_back("ok");
	return 0;
}

/*
 * TODO:new co
 */
/*Lua* Lua::ngx_http_lua_new_thread(lua_State *L){
    
}*/
