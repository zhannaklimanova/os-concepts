/**
 * @file sut.c
 * @author Zhanna Klimanova (zhanna.klimanova@mail.mcgill.ca)
 * @brief 
 * @version thread lib
 * @date 2022-10-16
 * 
 * @copyright Copyright (c) 2022
 * 
 */

/* -- Includes -- */
#include "sut.h"
#include "queue.h"
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>


// Use total thread count as a thread identification to differentiate them in queues
int threadCount;

// Main user thread
ucontext_t parent;

// Compute executor kernel level thread 1
pthread_t *cexec;

// I/O executor kernel level thread 2
pthread_t *iexec;

// Create mutex
pthread_mutex_t lock;

// Task ready queue: each running task is placed in ready queue for pickup by cexec
struct queue taskReadyQueue;

// Wait queue: i/o tasks that are waiting to be serviced
struct queue waitQueue;

// I/O-issued operation requests are placed in this queue; issuing task is placed in waitQueue
struct queue requestQueue; 

// Responses to I/O-issued operations; once received, task gets moved from waitQueue to taskReadyQueue
struct queue responseQueue;

void sut_init() 
{
    int rc;

    // Allocated memory for C-EXEC thread
    cexec = (pthread_t *)malloc(sizeof(pthread_t));

    // Allocate memory for I-EXEC thread
    iexec = (pthread_t *)malloc(sizeof(pthread_t));

    // State of mutex is initialized and unlocked
    rc = pthread_mutex_init(&lock, PTHREAD_MUTEX_DEFAULT);
    assert(rc == 0);

    //Allocate memory for the queues
    taskReadyQueue = queue_create();
    waitQueue = queue_create();
    requestQueue = queue_create();
    responseQueue = queue_create();
    
    //Initialize queues
    queue_init(&taskReadyQueue);
    queue_init(&waitQueue);
    queue_init(&requestQueue);
    queue_init(&responseQueue);

    // Tasks are run by the schedulers until they complete or yield
    // Create C-EXEC kernel thread
    rc = pthread_create(cexec, NULL, cexecScheduler, NULL); 
    assert(rc == 0);

    // Create I-EXEC kernel thread
    pthread_create(iexec, NULL, iexecScheduler, NULL);
    assert(rc == 0);
    
}

void *cexecScheduler()
{
    getcontext(&parent);
    const struct timespec sleepTime = {.tv_sec = 0, .tv_nsec = 100000000L};
    struct queue_entry *task;

    while (TRUE)
    {
        pthread_mutex_lock(&lock);
        task = queue_pop_head(&taskReadyQueue);
        pthread_mutex_unlock(&lock);

        if (task) { // new task was taken off of taskReadyQueue
            ucontext_t nextTaskContext = *(ucontext_t *)task->data;
            swapcontext(&parent, &nextTaskContext);
        }
        nanosleep(&sleepTime, NULL);
    }
    pthread_exit(NULL);
}

void *iexecScheduler()
{
    int y;
}

bool sut_create(sut_task_f fn)
{
	threaddesc *tdescptr = 
    tdescptr = malloc(sizeof(threaddesc)); // ??

    // Lock critical section to ensure more than multiple threads don't create tasks at the same time
    pthread_mutex_lock(&lock);
	getcontext(&(tdescptr->threadcontext));
	tdescptr->threadid = threadCount;
	tdescptr->threadstack = (char *)malloc(THREAD_STACK_SIZE);
	tdescptr->threadcontext.uc_stack.ss_sp = tdescptr->threadstack;
	tdescptr->threadcontext.uc_stack.ss_size = THREAD_STACK_SIZE;
	tdescptr->threadcontext.uc_link = NULL;
	tdescptr->threadcontext.uc_stack.ss_flags = 0;
	tdescptr->threadfunc = fn;
	makecontext(&(tdescptr->threadcontext), fn, 1, tdescptr);

	threadCount++;
    struct queue_entry *task = queue_new_node(&(tdescptr->threadcontext));
    queue_insert_tail(&taskReadyQueue, task);

    pthread_mutex_unlock(&lock);

	return (task) ? TRUE : FALSE;
}

void sut_yield() 
{
    ucontext_t current_context;
    getcontext(&current_context);
    struct queue_entry *task = queue_new_node(&current_context);

    // Save task into the task control block (taskReadyQueue)
    pthread_mutex_lock(&lock);
    queue_insert_tail(&taskReadyQueue, task);
    pthread_mutex_unlock(&lock);

    swapcontext(&current_context, &parent);
}

void sut_shutdown()
{
    int rc; 
    // Waiting for both threads to finish executing before termination
    // rc = pthread_join(cexec, NULL);
    // assert(rc == 0);

    // rc = pthread_join(iexec, NULL);
    // assert(rc == 0);
}

