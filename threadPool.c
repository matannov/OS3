#include "threadPool.h"
#include <stdio.h>
#include <stdlib.h>

#define kill_tp_on_NULL(thread_pool, nullable_var) if (nullable_var == NULL) { \
	osDestroyQueue(tp->task_queue); \
	free(tp->threads); \
	free(tp); \
	return NULL; \
}


/*
	This is the `main` routine that manages the thread pool 
	it waits for tasks to come in and assigns them to threads
*/
void tpThreadRunner(void* pool) {
	FuncAndParam* func_info = NULL;
	ThreadPool* tp = (ThreadPool*) pool;
	while (1) {
		if (tp->currently_dying) {
			return;
		}
		if (tp->work_available) {
			if (!pthread_mutex_lock(&(tp->task_queue_mutex))) {
				func_info = (FuncAndParam*) osDequeue(tp->task_queue);
				if (osIsQueueEmpty(tp->task_queue)) {
					tp->work_available = false;
				}
				pthread_mutex_unlock(&(tp->task_queue_mutex));
				pthread_cond_broadcast(&(tp->work_available_cond));
				func_info->computeFunc(func_info->param);
			}
			pthread_cond_wait(&(tp->work_available_cond), &(tp->work_available_cond_mutex));
			pthread_mutex_unlock(&(tp->work_available_cond_mutex));
		}
		else {
			pthread_cond_wait(&(tp->work_available_cond), &(tp->work_available_cond_mutex));
			pthread_mutex_unlock(&(tp->work_available_cond_mutex));
		}
	}
}


static void tpKiller(ThreadPool* threadPool) {
	pthread_cond_broadcast(&(threadPool->kill_cond));
}

ThreadPool* tpCreate(int numOfThreads) {
	int i = 0, j = 0, err = 0;
	ThreadPool* tp = (ThreadPool*)calloc(1, sizeof(*tp));
	if (tp == NULL) {
		return NULL;
	}
	
	tp->num_threads = numOfThreads;
	tp->work_available = false;
	tp->currently_dying = false;
	
	
	tp->task_queue = osCreateQueue();
	kill_tp_on_NULL(tp, tp->task_queue);
	
	pthread_mutex_init(&(tp->dying_tp_mutex), NULL);
	pthread_mutex_init(&(tp->task_queue_mutex), NULL);
	pthread_mutex_init(&(tp->work_available_cond_mutex), NULL);
	pthread_mutex_init(&(tp->kill_cond_mutex), NULL);
	pthread_cond_init(&(tp->work_available_cond), NULL);
	pthread_cond_init(&(tp->kill_cond), NULL);
	
	tp->threads = (pthread_t*)calloc(numOfThreads, sizeof(pthread_t*));
	for (i = 0; i < numOfThreads; i++) {
		err = pthread_create(tp->threads + i, NULL, (void *)tpThreadRunner, tp);
		if (err != 0) { 
			for (j = 0; j < i; j++) {
				pthread_cancel(tp->threads[j]);
			}
			kill_tp_on_NULL(tp, NULL);
		}
	}
	
	return tp;
}

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {
	int i = 0;
	FuncAndParam* func_info = (FuncAndParam*)calloc(1, sizeof(*func_info));
	func_info->computeFunc = (void*)tpKiller;
	func_info->param = threadPool;
	while (pthread_mutex_lock(&(threadPool->task_queue_mutex))) {   }
	if (!shouldWaitForTasks) {
		while (osDequeue(threadPool->task_queue)) {   }
	}
	osEnqueue(threadPool->task_queue, func_info);
	threadPool->work_available = true;
	pthread_mutex_unlock(&(threadPool->task_queue_mutex));
	pthread_cond_broadcast(&(threadPool->work_available_cond));
	pthread_cond_wait(&(threadPool->kill_cond), &(threadPool->kill_cond_mutex));
	threadPool->currently_dying = true;
	threadPool->work_available = false;
	
	osDestroyQueue(threadPool->task_queue);
	for (i = 0; i < threadPool->num_threads; i++) {
		 
		pthread_cond_broadcast(&(threadPool->work_available_cond));
		pthread_join(threadPool->threads[i], NULL);
	}
	pthread_mutex_destroy(&(threadPool->dying_tp_mutex));
	pthread_mutex_destroy(&(threadPool->task_queue_mutex));
	pthread_mutex_destroy(&(threadPool->work_available_cond_mutex));
	pthread_mutex_destroy(&(threadPool->kill_cond_mutex));
	pthread_cond_destroy(&(threadPool->work_available_cond));
	pthread_cond_destroy(&(threadPool->kill_cond));
	free(threadPool->threads);
	free(threadPool);	
	free(func_info);
}


int tpInsertTask(ThreadPool* threadPool, void (*computeFunc)(void *),
        void* param) {
	bool queue_was_empty = false;
	FuncAndParam* func_info = (FuncAndParam*)calloc(1, sizeof(*func_info)); // FIXME: when to free this??
	func_info->computeFunc = computeFunc;
	func_info->param = param;
	while (pthread_mutex_lock(&(threadPool->task_queue_mutex))) {   }
	osEnqueue(threadPool->task_queue, func_info);
	threadPool->work_available = true;
	pthread_mutex_unlock(&(threadPool->task_queue_mutex));
	pthread_cond_broadcast(&(threadPool->work_available_cond));
	return 0;
}

