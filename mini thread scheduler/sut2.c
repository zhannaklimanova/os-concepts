#include "sut2.h"
#include "queue.h"

// QUEUES
// Task ready queue: each running task is placed in ready queue for pickup by c_exec
struct queue taskReadyQueue;
// Wait queue: i/o tasks that are waiting to be serviced
struct queue waitQueue;

// THREADS
// Compute executor kernel level thread 1 and its context
pthread_t cexecThread;
ucontext_t cexecContext; // Can also be ucontext_t cexecContext (but needs to be referred with &cexecContext)
// I/O executor kernel level thread 2
pthread_t iexecThread;

// Task currently being executed by cexec
struct queue_entry *cexecCurrentRunningTask; // the cexecContext needs to be set with the context of this task

struct queue_entry *iexecCurrentRunningTask;

// Mutex for locking critical sections
pthread_mutex_t lock;

bool shutdown;

void sut_init()
{
    int rc;

    // Initialize mutex lock for handling future critical section
    rc = pthread_mutex_init(&lock, PTHREAD_MUTEX_DEFAULT);
    assert(rc == 0);

    // Create queues
    taskReadyQueue = queue_create();
    waitQueue = queue_create();

    // Initialize queues
    queue_init(&taskReadyQueue);
    queue_init(&waitQueue);

    // Capture context of main thread
    getcontext(&cexecContext); // Gets context of thread - N

    // Create C-EXEC kernel thread
    rc = pthread_create(&cexecThread, NULL, cexecScheduler, NULL);
    assert(rc == 0);
     // printf("Line Number %s->%s:%d\n", __FILE__, __FUNCTION__, __LINE__);

    // Create I-EXEC kernel thread
    rc = pthread_create(&iexecThread, NULL, iexecScheduler, NULL);
    assert(rc == 0);
}

void *cexecScheduler()
{
    struct timespec sleepTime;
    sleepTime.tv_sec = 0;
    sleepTime.tv_nsec = 100000; // 100 microseconds = 100,000 nanoseconds
    printf("in c exec");

    struct queue_entry *taskAtFrontOfReadyQueue;

    while(true)
    {
        pthread_mutex_lock(&lock);
        taskAtFrontOfReadyQueue = queue_peek_front(&taskReadyQueue);
        pthread_mutex_unlock(&lock);

        if (taskAtFrontOfReadyQueue != NULL) {
            cexecCurrentRunningTask = taskAtFrontOfReadyQueue;

            pthread_mutex_lock(&lock);
            queue_pop_head(&taskReadyQueue);
            pthread_mutex_unlock(&lock);

            swapcontext(&cexecContext, &(cexecCurrentRunningTask->context)); // Stalls and finishes task - N

            if (cexecCurrentRunningTask->isTaskActive == true) {
                pthread_mutex_lock(&lock);
                if (taskAtFrontOfReadyQueue->ioOperation.ioOperationType > 0) {
                    queue_insert_tail(&waitQueue, taskAtFrontOfReadyQueue); // I/O operation requested by task
                }
                else {
                    queue_insert_tail(&taskReadyQueue, taskAtFrontOfReadyQueue); // Insert the task at the back of ready queue
                }
                pthread_mutex_unlock(&lock);
            }
            else {
                free(cexecCurrentRunningTask);
            }
        }
        else {
            pthread_mutex_lock(&lock);
            struct queue_entry *readyTask = queue_peek_front(&taskReadyQueue);
            struct queue_entry *waitTask = queue_peek_front(&waitQueue);
            pthread_mutex_unlock(&lock);
            if (readyTask == NULL && waitTask == NULL && shutdown == true && cexecCurrentRunningTask == NULL && iexecCurrentRunningTask == NULL) {
                break;
            }
        }
        cexecCurrentRunningTask = NULL;
        nanosleep(&sleepTime, NULL);
    }
    pthread_exit(NULL);
}

void *iexecScheduler()
{
    struct timespec sleepTime;
    sleepTime.tv_sec = 0;
    sleepTime.tv_nsec = 100000; // 100 microseconds = 100,000 nanoseconds
    printf("in i exec");

    struct queue_entry *taskAtFrontOfWaitQueue;

    while(true)
    {
        pthread_mutex_lock(&lock);
        taskAtFrontOfWaitQueue = queue_peek_front(&waitQueue);
        pthread_mutex_unlock(&lock);

        if (taskAtFrontOfWaitQueue != NULL) {
            iexecCurrentRunningTask = taskAtFrontOfWaitQueue;

            pthread_mutex_lock(&lock);
            queue_pop_head(&waitQueue);
            pthread_mutex_unlock(&lock);

            switch (taskAtFrontOfWaitQueue->ioOperation.ioOperationType)
            {
                case OPEN_OP: {
                    struct openFile *taskInfo = &(taskAtFrontOfWaitQueue->ioOperation.ioOperationData.openFile); // ? b/c isn't it already an address by default since its pointer??????
                    taskInfo->returnVal = open(taskInfo->fname, O_RDWR|O_CREAT|O_WRONLY|O_TRUNC);
                    break;
                }
                case READ_OP: {
                    /*
                        buf: pre-allocated memory buffer provided to sut_read function
                        size: tells function the max number of bytes that could be copied into the buffer
                    */
                    struct readFile *taskInfo = &(taskAtFrontOfWaitQueue->ioOperation.ioOperationData.readFile);
                    if (read(taskInfo->fd, taskInfo->buf, taskInfo->size) >= 0) {
                        taskInfo->returnVal = taskInfo->buf; // function returns a non NULL value on success
                    }
                    else {
                        taskInfo->returnVal = taskInfo->buf;
                    }
                    break;
                }
                case WRITE_OP: {
                    // Write the bytes in buf to the disk file that is already open.
                    struct writeFile *taskInfo = &(taskAtFrontOfWaitQueue->ioOperation.ioOperationData.writeFile);
                    write(taskInfo->fd, taskInfo->buf, taskInfo->size);
                    break;
                }
                case CLOSE_OP: {
                    // Close file pointed to by fd
                    struct closeFile *taskInfo = &(taskAtFrontOfWaitQueue->ioOperation.ioOperationData.closeFile);
                    close(taskInfo->fd);
                    break;
                }
            }
            // Reset the I/O operation type as none since it was just performed (the information from I/O stays in struct)
            taskAtFrontOfWaitQueue->ioOperation.ioOperationType = NO_IO_OP_REQUESTED;

            // Once IO operation has finished, put the task at the end of the readyTaskQueue
            pthread_mutex_lock(&lock);
            queue_insert_tail(&taskReadyQueue, taskAtFrontOfWaitQueue);
            pthread_mutex_unlock(&lock);
        }
        else {
            pthread_mutex_lock(&lock);
            struct queue_entry *readyTask = queue_peek_front(&taskReadyQueue);
            struct queue_entry *waitTask = queue_peek_front(&waitQueue);
            pthread_mutex_unlock(&lock);
            if (readyTask == NULL && waitTask == NULL && shutdown == true && cexecCurrentRunningTask == NULL && iexecCurrentRunningTask == NULL) {
                break;
            }
        }
        iexecCurrentRunningTask = NULL;
        nanosleep(&sleepTime, NULL);
    }
    pthread_exit(NULL);
}

bool sut_create(sut_task_f fn)
{
    struct queue_entry *newTask = NULL;
    newTask = (struct queue_entry *)malloc(sizeof(struct queue_entry));

    getcontext(&(newTask->context));
    newTask->context.uc_stack.ss_sp = newTask->stack;
    newTask->context.uc_stack.ss_size = THREAD_STACK_SIZE;
    newTask->context.uc_link = NULL;
    newTask->context.uc_stack.ss_flags = 0;
    newTask->isTaskActive = true;
    makecontext(&newTask->context, fn, 0);

    if (newTask == NULL) {
        return false;
    }

    pthread_mutex_lock(&lock);
    queue_insert_tail(&taskReadyQueue, newTask);
    pthread_mutex_unlock(&lock);

    return true;
}

void sut_yield()
{
    swapcontext(&cexecCurrentRunningTask->context, &cexecContext);
}

void sut_exit()
{
    cexecCurrentRunningTask->isTaskActive = false;
    sut_yield();
}

int sut_open(char *fname)
{
    cexecCurrentRunningTask->ioOperation.ioOperationType = OPEN_OP;
    cexecCurrentRunningTask->ioOperation.ioOperationData.openFile.fname = fname;
    sut_yield(); // switch to cexec thread, which will ask iothread to perform the operation to will insert the return value into struct
    return cexecCurrentRunningTask->ioOperation.ioOperationData.openFile.returnVal;
}

char *sut_read(int fd, char *buf, int size)
{
    cexecCurrentRunningTask->ioOperation.ioOperationType = READ_OP;
    cexecCurrentRunningTask->ioOperation.ioOperationData.readFile.fd = fd;
    cexecCurrentRunningTask->ioOperation.ioOperationData.readFile.buf = buf;
    cexecCurrentRunningTask->ioOperation.ioOperationData.readFile.size = size;
    sut_yield();
    return cexecCurrentRunningTask->ioOperation.ioOperationData.readFile.returnVal;
}

void sut_write(int fd, char *buf, int size)
{
    cexecCurrentRunningTask->ioOperation.ioOperationType = WRITE_OP;
    cexecCurrentRunningTask->ioOperation.ioOperationData.writeFile.fd = fd;
    cexecCurrentRunningTask->ioOperation.ioOperationData.writeFile.buf = buf;
    cexecCurrentRunningTask->ioOperation.ioOperationData.writeFile.size = size;
    sut_yield();
}

void sut_close(int fd)
{
    cexecCurrentRunningTask->ioOperation.ioOperationType = CLOSE_OP;
    cexecCurrentRunningTask->ioOperation.ioOperationData.writeFile.fd = fd;
    sut_yield();
}

void sut_shutdown()
{
    shutdown = true;
    pthread_join(cexecThread, NULL);
    pthread_join(iexecThread, NULL);
}