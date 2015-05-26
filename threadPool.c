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

static int dead_threads = 0;

void tpThreadRunner(void* pool) {
	FuncAndParam* func_info = NULL;
	ThreadPool* tp = (ThreadPool*) pool;
	while (1) {
		printf("while 1\n");
		if (tp->currently_dying) {
			printf("dead thread %d\n", dead_threads++);
			return;
		}
		if (tp->work_available && !pthread_mutex_lock(&(tp->task_queue_mutex))) {
			func_info = (FuncAndParam*) osDequeue(tp->task_queue);
			if (osIsQueueEmpty(tp->task_queue)) {
				tp->work_available = false;
			}
			pthread_mutex_unlock(&(tp->task_queue_mutex));
			pthread_cond_broadcast(&(tp->work_available_cond));
			func_info->computeFunc(func_info->param);
			free(func_info);
		}
		while ( pthread_mutex_lock(&(tp->work_available_cond_mutex)) ) { printf("while 2\n"); }
		printf("waiting for job 1\n");
		pthread_cond_wait(&(tp->work_available_cond), &(tp->work_available_cond_mutex));
		pthread_mutex_unlock(&(tp->work_available_cond_mutex));
		printf("finish waiting for job \n");
	}
}


static void tpKiller(ThreadPool* threadPool) {
	printf("killer instinct\n");
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
		printf("made a thread!\n");
		if (err != 0) { 
			printf("we're fucked");
			for (j = 0; j < i; j++) {
				pthread_cancel(tp->threads[j]);
			}
			kill_tp_on_NULL(tp, NULL);
		}
	}
	
	printf("made tp\n");
	
	return tp;
}

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {
	int i = 0;
	FuncAndParam* func_info = (FuncAndParam*)calloc(1, sizeof(*func_info));
	func_info->computeFunc = (void*)tpKiller;
	func_info->param = threadPool;
	while (pthread_mutex_lock(&(threadPool->task_queue_mutex))) {  printf("while 3\n"); } // must be first to get lock
	if (!shouldWaitForTasks) {
		while (osDequeue(threadPool->task_queue)) { printf("while 4\n");  }
	}
	printf("destroy stage1 \n");
	
	osEnqueue(threadPool->task_queue, func_info);
	threadPool->work_available = true;
	pthread_mutex_unlock(&(threadPool->task_queue_mutex));
	pthread_cond_broadcast(&(threadPool->work_available_cond));
	printf("destroy stage2 \n");
	pthread_mutex_lock(&(threadPool->kill_cond_mutex));
	printf("waiting 2\n");
	pthread_cond_wait(&(threadPool->kill_cond), &(threadPool->kill_cond_mutex));
	printf("finish waiting 2\n");
	threadPool->currently_dying = true;
	
	osDestroyQueue(threadPool->task_queue);
	for (i = 0; i < threadPool->num_threads; i++) {
		pthread_cond_broadcast(&(threadPool->work_available_cond));
		printf("joinin1 \n");
		pthread_join(threadPool->threads[i], NULL);
		printf("finish join \n");
	}
	
	pthread_mutex_destroy(&(threadPool->dying_tp_mutex));
	pthread_mutex_destroy(&(threadPool->task_queue_mutex));
	pthread_mutex_destroy(&(threadPool->work_available_cond_mutex));
	pthread_mutex_destroy(&(threadPool->kill_cond_mutex));
	pthread_cond_destroy(&(threadPool->work_available_cond));
	pthread_cond_destroy(&(threadPool->kill_cond));
	free(threadPool->threads);
	free(threadPool);
	printf("destroy stage9 \n");
}


int tpInsertTask(ThreadPool* threadPool, void (*computeFunc)(void *),
        void* param) {
	bool queue_was_empty = false;
	FuncAndParam* func_info = (FuncAndParam*)calloc(1, sizeof(*func_info));
	func_info->computeFunc = computeFunc;
	func_info->param = param;
	while (pthread_mutex_lock(&(threadPool->task_queue_mutex))) { printf("while 5\n"); } // FIXME: should not be busy wait
	osEnqueue(threadPool->task_queue, func_info);
	threadPool->work_available = true;
	pthread_mutex_unlock(&(threadPool->task_queue_mutex));
	pthread_cond_broadcast(&(threadPool->work_available_cond));
	return 0;
}

