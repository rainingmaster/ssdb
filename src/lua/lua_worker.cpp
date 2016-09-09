#include "../util/log.h"
#include "../include.h"
#include "lua_worker.h"

LuaWorker::LuaWorker(const std::string &name)
{
	this->name = name;
}

void LuaWorker::init()
{
	log_debug("%s %d init", this->name.c_str(), this->id);
    lua = new LuaHandler();
}

int LuaWorker::proc(LuaJob *job)
{
	lua->lua_set_ssdb_resp(job->resp);
	lua->lua_set_ssdb_serv((SSDBServer *)job->serv->data);

	lua->lua_execute_by_filepath(&(job->filepath));

    ProcJob *pjob = new ProcJob();
    pjob->link = job->link;
    pjob->resp = *(job->resp);
    pjob->req  = &(job->req);

    if(job->link->send(job->resp->resp) == -1){
		pjob->result = PROC_ERROR;
	}else{
        int len = job->link->write();
        if(len < 0){
            pjob->result = PROC_ERROR;
        }
	}
    job->serv->writer->insert(pjob);

	return 0;
}
