#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <pthread.h>
#include <stdbool.h>
#include "osqueue.h"

typedef struct thread_pool {
	int	 num_threads;
	bool currently_dying, work_available;
	
	pthread_t* threads;
	OSQueue* task_queue;
	
	pthread_mutex_t dying_tp_mutex;
	pthread_mutex_t task_queue_mutex;
	
	pthread_cond_t work_available_cond;
	pthread_mutex_t work_available_cond_mutex;
	
	pthread_cond_t kill_cond;
	pthread_mutex_t kill_cond_mutex;
} ThreadPool;

typedef struct funcAndParam {
	void* param;
	void (*computeFunc)(void *);
} FuncAndParam;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc)(void *),
        void* param);

#endif
