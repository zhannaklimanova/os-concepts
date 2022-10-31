#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "sut.h"
#include "queue.h"

// Use total thread count as a thread identification to differentiate them in queues
int threadCount;

// Compute executor kernel level thread 1
pthread_t *cexec;

// I/O executor kernel level thread 2
pthread_t *iexec;

// C-EXEC Mutex
pthread_mutex_t lock;

// Main user thread
ucontext_t parent;

// Task ready queue: each running task is placed in ready queue for pickup by c_exec
struct queue taskReadyQueue;

// Wait queue: i/o tasks that are waiting to be serviced
struct queue waitQueue;

// I/O-issued operation requests are placed in this queue; issuing task is placed in waitQueue
struct queue requestQueue;

// Responses to I/O-issued operations; once received, task gets moved from waitQueue to taskReadyQueue
struct queue responseQueue;

void sut_init() {
  int rc;

  // Number of threads counter
  threadCount = 0;

  // Allocated memory for C-EXEC thread
  cexec = (pthread_t *)malloc(sizeof(pthread_t));
  // Allocate memory for I-EXEC thread
  iexec = (pthread_t *)malloc(sizeof(pthread_t));

  // Initialize mutex lock for handling future critical section
  rc = pthread_mutex_init(&lock, PTHREAD_MUTEX_DEFAULT);
  assert(rc == 0);

  // Create queues
  taskReadyQueue = queue_create();
  waitQueue = queue_create();
  requestQueue = queue_create();
  responseQueue = queue_create();

  // Initialize queues
  queue_init(&taskReadyQueue);
  queue_init(&waitQueue);
  queue_init(&requestQueue);
  queue_init(&responseQueue);

  // Capture context of main
  // getcontext(&parent);
  // printf("Line Number %s->%s:%d\n", __FILE__, __FUNCTION__, __LINE__);

  // Create C-EXEC kernel thread
  rc = pthread_create(cexec, NULL, c_exec, NULL);
  assert(rc == 0);
  // printf("Line Number %s->%s:%d\n", __FILE__, __FUNCTION__, __LINE__);


  // Create I-EXEC kernel thread
  // pthread_create(iexec, NULL, iexec_scheduler, NULL);
  // printf("Line Number %s->%s:%d\n", __FILE__, __FUNCTION__, __LINE__);

}

void *c_exec()
{
  struct timespec sleepTime; // https://www.educative.io/answers/what-is-timespec-in-c
  sleepTime.tv_sec = 0;
  sleepTime.tv_nsec = 100000; // 100 microseconds = 100,000 nanoseconds
  // printf("Line Number %s->%s:%d\n", __FILE__, __FUNCTION__, __LINE__);

  pthread_mutex_lock(&lock);
  struct queue_entry *readyTask = queue_peek_front(&taskReadyQueue);
  struct queue_entry *waitTask = queue_peek_front(&waitQueue);
  pthread_mutex_unlock(&lock);
  // printf("Line Number %s->%s:%d\n", __FILE__, __FUNCTION__, __LINE__);

  // printf("Line Number %s->%s:%d\n", __FILE__, __FUNCTION__, __LINE__);

  while (readyTask != NULL || waitTask != NULL) // readyTask != NULL || waitTask != NULL
  {
    // printf("Line Number %s->%s:%d\n", __FILE__, __FUNCTION__, __LINE__);

    pthread_mutex_lock(&lock);
    readyTask = queue_peek_front(&taskReadyQueue);
    pthread_mutex_unlock(&lock);
    // printf("Line Number %s->%s:%d\n", __FILE__, __FUNCTION__, __LINE__);

    if (readyTask) { // if there's a task available in tasksReadyQueue then context can be switched
      // printf("Line Number %s->%s:%d\n", __FILE__, __FUNCTION__, __LINE__);

      pthread_mutex_lock(&lock);
      queue_pop_head(&taskReadyQueue); // remove task from the queue because it is being executed in context
      pthread_mutex_unlock(&lock);
      // printf("Line Number %s->%s:%d\n", __FILE__, __FUNCTION__, __LINE__);

      // ucontext_t context = *(ucontext_t *)readyTask->data;
      swapcontext(&parent, readyTask->data);
      // printf("Line Number %s->%s:%d\n", __FILE__, __FUNCTION__, __LINE__);

    }
    pthread_mutex_lock(&lock);
    readyTask = queue_peek_front(&taskReadyQueue);
    waitTask = queue_peek_front(&waitQueue);
    pthread_mutex_unlock(&lock);

    nanosleep(&sleepTime, NULL);
  }
  pthread_exit(NULL);
}

/*--- sut_create ---*/
bool sut_create(sut_task_f fn) {

  /*
  A newly created task is
  added to the end of the task ready queue.
  */
  // thread descriptor
  threaddesc *tdescptr;
  tdescptr = malloc(sizeof(threaddesc));

  // Create user-level thread
  getcontext(&(tdescptr->threadcontext)); // save current user-level thread context
  tdescptr->threadid = threadCount;
  tdescptr->threadstack = (char *)malloc(THREAD_STACK_SIZE);
  tdescptr->threadcontext.uc_stack.ss_sp = tdescptr->threadstack; // uc_stack : Stack used for this context
  tdescptr->threadcontext.uc_stack.ss_size = THREAD_STACK_SIZE;
  tdescptr-> threadcontext.uc_link = 0; // &parent - uc_link : This is a pointer to the 'parent' context structure which is used if the context described in the current structure returns.
  tdescptr-> threadcontext.uc_stack.ss_flags = 0;
  // tdescptr->threadfunc = &fn;

  makecontext(&(tdescptr->threadcontext), fn, 0);

  // increment number of user-level threads as we just created a new one
  threadCount++;

  // Add thread to back of taskReadyQueue (atomic)
  struct queue_entry *task = queue_new_node(&(tdescptr->threadcontext));
  pthread_mutex_lock(&lock);
  queue_insert_tail(&taskReadyQueue, task); // newly created task is added to end of task ready queue
  pthread_mutex_unlock(&lock);

  return 0;
}

/*--- sut_yield ---*/
void sut_yield() {

  // Get current context
  ucontext_t current_context;
  getcontext(&current_context);
  struct queue_entry *task = queue_new_node(&current_context);

  /*---
  // A task that is yielding is put back at the end of the ready queue and
  // the task at the front of the queue is selected to run next by the scheduler.
  */
  // Put task back at end of taskReadyQueue
  pthread_mutex_lock(&lock);
  queue_insert_tail(&taskReadyQueue, task);
  pthread_mutex_unlock(&lock);

  // swap content from current to c-exec
  swapcontext(&current_context, &parent);
}


/*--- sut_exit ---*/
void sut_exit() {

  // Stop execution of current task
  // and DO NOT put task back of taskReadyQueue
  ucontext_t current;
  getcontext(&current);
  swapcontext(&current, &parent);
}

/*--- sut_shutdown ---*/
void sut_shutdown() { // The  executors terminate only after executing all tasks in the queues
  // We need to keep the main thread waiting for the C-EXEC and I-EXEC threads
  // and it one of the important functions of sut_shutdown().
  // In addition, you can put any termination related actions into this function and cleanly terminate the threading library
  pthread_join(*cexec, NULL);
  pthread_join(*iexec, NULL);
}
