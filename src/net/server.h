/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
#ifndef NET_SERVER_H_
#define NET_SERVER_H_

#include "../include.h"
#include <string>
#include <vector>

#include "fde.h"
#include "proc.h"
#include "worker.h"
#include "../lua/lua.h"

class Link;
class Config;
class IpFilter;
class Fdevents;
class Lua;

typedef std::vector<Link *> ready_list_t;

class NetworkServer
{
private:
	int tick_interval;
	int status_report_ticks;

	//Config *conf;
	Link *serv_link;
	Fdevents *fdes;

	Link* accept_link();
	int proc_result(ProcJob *job, ready_list_t *ready_list);
	int proc_client_event(const Fdevent *fde, ready_list_t *ready_list);

	int proc(ProcJob *job);

	int num_readers;
	int num_writers;

	NetworkServer();

protected:
	void usage(int argc, char **argv);

public:
	IpFilter *ip_filter;
	void *data;
	ProcMap proc_map;
	int link_count;
	bool need_auth;
	std::string password;
    
    Lua *hlua;
	ProcWorkerPool *writer;
	ProcWorkerPool *reader;

	~NetworkServer();
	
	// could be called only once
	static NetworkServer* init(const char *conf_file, int num_readers=-1, int num_writers=-1);
	static NetworkServer* init(const Config &conf, int num_readers=-1, int num_writers=-1);
	void serve();
};


#endif
