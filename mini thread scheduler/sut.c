#include "sut.h"
#include "queue.h"

// QUEUES
// Task ready queue: each running task is placed in ready queue for pickup by c_exec
struct queue taskReadyQueue;
// Wait queue: i/o tasks that are waiting to be serviced
struct queue waitQueue;

// THREADS
// Compute executor kernel level thread 1 and its context
pthread_t *cexecThread;
ucontext_t cexecContext;
// I/O executor kernel level thread 2
pthread_t *iexecThread;

// CURRENT TASKS IN EACH THREAD
struct queue_entry *cexecCurrentRunningTask; // the cexecContext needs to be set with the context of this task
struct queue_entry *iexecCurrentRunningTask;

// Mutex for locking critical sections
pthread_mutex_t lock;

// Checking to see if task has requested the sut_shutdown operation
bool isShutdown;

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

    // Capture and initialize main context of the cexec thread
    getcontext(&cexecContext);

    // Create C-EXEC kernel thread
    cexecThread = (pthread_t *)malloc(sizeof(pthread_t));
    rc = pthread_create(cexecThread, NULL, cexecScheduler, NULL);
    assert(rc == 0);

    // Create I-EXEC kernel thread
    iexecThread = (pthread_t *)malloc(sizeof(pthread_t));
    rc = pthread_create(iexecThread, NULL, iexecScheduler, NULL);
    assert(rc == 0);

    // Initialize current running task pointers
    cexecCurrentRunningTask = (struct queue_entry *)malloc(sizeof(struct queue_entry));
    iexecCurrentRunningTask = (struct queue_entry *)malloc(sizeof(struct queue_entry));
}

void *cexecScheduler()
{
    struct timespec sleepTime;
    sleepTime.tv_sec = 0;
    sleepTime.tv_nsec = 100000; // 100 microseconds = 100,000 nanoseconds

    struct queue_entry *taskAtFrontOfReadyQueue = (struct queue_entry *)malloc(sizeof(struct queue_entry));

    while(true)
    {
        pthread_mutex_lock(&lock);
        taskAtFrontOfReadyQueue = queue_peek_front(&taskReadyQueue);
        pthread_mutex_unlock(&lock);

        if (taskAtFrontOfReadyQueue != NULL) {
            cexecCurrentRunningTask = taskAtFrontOfReadyQueue; // take task from ready queue and put it as the one being run by cexec thread

            pthread_mutex_lock(&lock); // lock critical sections (anything todo with queue manipulation is a critical section)
            queue_pop_head(&taskReadyQueue); // pop task to perform some operations on it
            pthread_mutex_unlock(&lock);

            swapcontext(&cexecContext, &(cexecCurrentRunningTask->context)); // switches to task and stalls until task is either finished or is yielding

            // The cexecCurrentRunningTask has either finished or is yielding somewhere
            if (cexecCurrentRunningTask->isTaskActive == true) { // task has not finished
                pthread_mutex_lock(&lock);
                if (taskAtFrontOfReadyQueue->ioOperation.ioOperationType > 0) {
                    // I/O operation requested by task so its placed at end of wait queue
                    queue_insert_tail(&waitQueue, taskAtFrontOfReadyQueue);
                }
                else {
                    // A task that is yielding is put back at the end of task ready queue
                    queue_insert_tail(&taskReadyQueue, taskAtFrontOfReadyQueue);
                }
                pthread_mutex_unlock(&lock);
            }
        }
        else {
            pthread_mutex_lock(&lock);
            struct queue_entry *readyTask = queue_peek_front(&taskReadyQueue);
            struct queue_entry *waitTask = queue_peek_front(&waitQueue);
            pthread_mutex_unlock(&lock);
            if (readyTask == NULL && waitTask == NULL && isShutdown == true && cexecCurrentRunningTask == NULL && iexecCurrentRunningTask == NULL) {
                free(taskAtFrontOfReadyQueue);
                pthread_exit(NULL);
            }
        }
        cexecCurrentRunningTask = NULL;
        nanosleep(&sleepTime, NULL);
    }
    free(taskAtFrontOfReadyQueue);
    pthread_exit(NULL);
}

void *iexecScheduler()
{
    struct timespec sleepTime;
    sleepTime.tv_sec = 0;
    sleepTime.tv_nsec = 100000; // 100 microseconds = 100,000 nanoseconds

    struct queue_entry *taskAtFrontOfWaitQueue = (struct queue_entry *)malloc(sizeof(struct queue_entry));

    while(true)
    {
        pthread_mutex_lock(&lock);
        taskAtFrontOfWaitQueue = queue_peek_front(&waitQueue);
        pthread_mutex_unlock(&lock);

        if (taskAtFrontOfWaitQueue != NULL) {
            iexecCurrentRunningTask = taskAtFrontOfWaitQueue; // take task from wait queue and put it as the one being run by iexec thread

            pthread_mutex_lock(&lock);
            queue_pop_head(&waitQueue); // pop task to perform some operations on it
            pthread_mutex_unlock(&lock);

            switch (taskAtFrontOfWaitQueue->ioOperation.ioOperationType)
            {
                case OPEN_OP: {
                    int rc;
                    mode_t permissions = 0770;
                    struct openFile *taskInfo = &(taskAtFrontOfWaitQueue->ioOperation.ioOperationData.openFile);

                    rc = open(taskInfo->fname, O_RDWR|O_CREAT, permissions);
                    if (rc >= 0) {
                        taskInfo->returnVal = rc;
                    }
                    break;
                }
                case READ_OP: {
                    /*
                        buf: this buffer is used a pre-allocated memory space provided by stack to store information from fd
                        size: tells function the max number of bytes that could be copied into the buffer
                    */
                    struct readFile *taskInfo = &(taskAtFrontOfWaitQueue->ioOperation.ioOperationData.readFile);
                    if (read(taskInfo->fd, taskInfo->buf, taskInfo->size) >= 0) { // assuming these parameters have been intialized by sut_read
                        taskInfo->returnVal = taskInfo->buf; // function returns a non NULL value on success
                    }
                    else {
                        taskInfo->returnVal = NULL;
                    }
                    break;
                }
                case WRITE_OP: {
                    // Write the bytes in buf to the disk file that is already open
                    struct writeFile *taskInfo = &(taskAtFrontOfWaitQueue->ioOperation.ioOperationData.writeFile);
                    write(taskInfo->fd, taskInfo->buf, taskInfo->size); // not considering write errors in this call
                    break;
                }
                case CLOSE_OP: {
                    // Close file pointed to by fd
                    int rc = 0;
                    struct closeFile *taskInfo = &(taskAtFrontOfWaitQueue->ioOperation.ioOperationData.closeFile);
                    rc = close(taskInfo->fd);
                    if (rc == -1) {
                        printf("%s", "close: error closing the file pointed to by file descriptor.\n");
                    }
                    break;
                }
            }
            // Reset the I/O operation type as none since it was just performed (the information from I/O stays in struct for future use however)
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
            if (readyTask == NULL && waitTask == NULL && isShutdown == true && cexecCurrentRunningTask == NULL && iexecCurrentRunningTask == NULL) {
                free(taskAtFrontOfWaitQueue);
                pthread_exit(NULL);
            }
        }
        iexecCurrentRunningTask = NULL;
        nanosleep(&sleepTime, NULL);
    }
    free(taskAtFrontOfWaitQueue);
    pthread_exit(NULL);
}

bool sut_create(sut_task_f fn)
{
    struct queue_entry *newTask = (struct queue_entry *)malloc(sizeof(struct queue_entry));

    getcontext(&(newTask->context));
    newTask->context.uc_stack.ss_sp = newTask->stack;
    newTask->context.uc_stack.ss_size = THREAD_STACK_SIZE;
    newTask->context.uc_link = NULL;
    newTask->context.uc_stack.ss_flags = 0;
    newTask->isTaskActive = true;
    newTask->ioOperation.ioOperationType = NO_IO_OP_REQUESTED;
    makecontext(&newTask->context, fn, 0);

    if (newTask == NULL) {
        return false;
    }

    // Newly created task is added at the end of the task ready queue
    pthread_mutex_lock(&lock);
    queue_insert_tail(&taskReadyQueue, newTask);
    pthread_mutex_unlock(&lock);

    return true;
}

void sut_yield()
{
    // After swapping context, cexecThread returns to where it was last executing (most likely the line just after running swapcontext in cexecScheduler)
    swapcontext(&cexecCurrentRunningTask->context, &cexecContext);
}

void sut_exit()
{
    cexecCurrentRunningTask->isTaskActive = false;
    swapcontext(&cexecCurrentRunningTask->context, &cexecContext);
}

int sut_open(char *fname)
{
    cexecCurrentRunningTask->ioOperation.ioOperationType = OPEN_OP;
    cexecCurrentRunningTask->ioOperation.ioOperationData.openFile.fname = fname;
    swapcontext(&cexecCurrentRunningTask->context, &cexecContext); // switch to cexecThread, which will ask iexecThread to perform the open() system call

    return cexecCurrentRunningTask->ioOperation.ioOperationData.openFile.returnVal;
}

char *sut_read(int fd, char *buf, int size)
{
    cexecCurrentRunningTask->ioOperation.ioOperationType = READ_OP;
    cexecCurrentRunningTask->ioOperation.ioOperationData.readFile.fd = fd;
    cexecCurrentRunningTask->ioOperation.ioOperationData.readFile.buf = buf;
    cexecCurrentRunningTask->ioOperation.ioOperationData.readFile.size = size;
    swapcontext(&cexecCurrentRunningTask->context, &cexecContext);

    return cexecCurrentRunningTask->ioOperation.ioOperationData.readFile.returnVal;
}

void sut_write(int fd, char *buf, int size)
{
    cexecCurrentRunningTask->ioOperation.ioOperationType = WRITE_OP;
    cexecCurrentRunningTask->ioOperation.ioOperationData.writeFile.fd = fd;
    cexecCurrentRunningTask->ioOperation.ioOperationData.writeFile.buf = buf;
    cexecCurrentRunningTask->ioOperation.ioOperationData.writeFile.size = size;
    swapcontext(&cexecCurrentRunningTask->context, &cexecContext);
}

void sut_close(int fd)
{
    cexecCurrentRunningTask->ioOperation.ioOperationType = CLOSE_OP;
    cexecCurrentRunningTask->ioOperation.ioOperationData.writeFile.fd = fd;
    swapcontext(&cexecCurrentRunningTask->context, &cexecContext);
}

void sut_shutdown()
{
    isShutdown = true;

    pthread_join(*cexecThread, NULL);
    pthread_join(*iexecThread, NULL);

    free(cexecCurrentRunningTask);
    free(iexecCurrentRunningTask);
    free(cexecThread);
    free(iexecThread);
}