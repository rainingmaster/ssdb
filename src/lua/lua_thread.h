#ifndef LUA_LTHREAD_H_
#define LUA_LTHREAD_H_

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <queue>
#include <vector>

// Thread safe queue
template <class T>
class LQueue{
	private:
		pthread_cond_t cond;
		pthread_mutex_t mutex;
		std::queue<T> items;
	public:
		LQueue();
		~LQueue();

		bool empty();
		int size();
		int push(const T item);
		// TODO: with timeout
		int pop(T *data);
};

template<class W, class JOB>
class LWorkerPool{
	public:
		class Worker{
			public:
				Worker(){};
				Worker(const std::string &name);
				virtual ~Worker(){}
				int id;
				virtual void init(){}
				virtual void destroy(){}
				virtual int proc(JOB job) = 0;
			private:
			protected:
				std::string name;
		};
	private:
		std::string name;
		LQueue<JOB> jobs;

		int num_workers;
		std::vector<pthread_t> tids;
		bool started;

		struct run_arg{
			int id;
			LWorkerPool *tp;
		};
		static void* _run_worker(void *arg);
	public:
		LWorkerPool(const char *name="");
		~LWorkerPool();
		
		int start(int num_workers);
		int stop();
		
		int push(JOB job);
		int pop(JOB *job);
};



template <class T>
LQueue<T>::LQueue(){
	pthread_cond_init(&cond, NULL);
	pthread_mutex_init(&mutex, NULL);
}

template <class T>
LQueue<T>::~LQueue(){
	pthread_cond_destroy(&cond);
	pthread_mutex_destroy(&mutex);
}

template <class T>
bool LQueue<T>::empty(){
	bool ret = false;
	if(pthread_mutex_lock(&mutex) != 0){
		return -1;
	}
	ret = items.empty();
	pthread_mutex_unlock(&mutex);
	return ret;
}

template <class T>
int LQueue<T>::size(){
	int ret = -1;
	if(pthread_mutex_lock(&mutex) != 0){
		return -1;
	}
	ret = items.size();
	pthread_mutex_unlock(&mutex);
	return ret;
}

template <class T>
int LQueue<T>::push(const T item){
	if(pthread_mutex_lock(&mutex) != 0){
		return -1;
	}
	{
		items.push(item);
	}
	pthread_mutex_unlock(&mutex);
	pthread_cond_signal(&cond);
	return 1;
}

template <class T>
int LQueue<T>::pop(T *data){
	if(pthread_mutex_lock(&mutex) != 0){
		return -1;
	}
	{
		while(items.empty()){
			if(pthread_cond_wait(&cond, &mutex) != 0){
				return -1;
			}
		}
		*data = items.front();
		items.pop();
	}
	if(pthread_mutex_unlock(&mutex) != 0){
		return -1;
	}
	return 1;
}


template<class W, class JOB>
LWorkerPool<W, JOB>::LWorkerPool(const char *name){
	this->name = name;
	this->started = false;
}

template<class W, class JOB>
LWorkerPool<W, JOB>::~LWorkerPool(){
	if(started){
		stop();
	}
}

template<class W, class JOB>
int LWorkerPool<W, JOB>::push(JOB job){
	return this->jobs.push(job);
}

template<class W, class JOB>
void* LWorkerPool<W, JOB>::_run_worker(void *arg){
	struct run_arg *p = (struct run_arg*)arg;
	int id = p->id;
	LWorkerPool *tp = p->tp;
	delete p;

	W w(tp->name);
	Worker *worker = (Worker *)&w;
	worker->id = id;
	worker->init();
	while(1){
		JOB job;
		if(tp->jobs.pop(&job) == -1){
			fprintf(stderr, "jobs.pop error\n");
			::exit(0);
			break;
		}
		worker->proc(job);
	}
	worker->destroy();
	return (void *)NULL;
}

template<class W, class JOB>
int LWorkerPool<W, JOB>::start(int num_workers){
	this->num_workers = num_workers;
	if(started){
		return 0;
	}
	int err;
	pthread_t tid;
	for(int i=0; i<num_workers; i++){
		struct run_arg *arg = new run_arg();
		arg->id = i;
		arg->tp = this;

		err = pthread_create(&tid, NULL, &LWorkerPool::_run_worker, arg);
		if(err != 0){
			fprintf(stderr, "can't create thread: %s\n", strerror(err));
		}else{
			tids.push_back(tid);
		}
	}
	started = true;
	return 0;
}

template<class W, class JOB>
int LWorkerPool<W, JOB>::stop(){
	// TODO: notify works quit and wait
	for(int i=0; i<tids.size(); i++){
#ifdef OS_ANDROID
#else
		pthread_cancel(tids[i]);
#endif
	}
	started = false;
	return 0;
}

#endif


