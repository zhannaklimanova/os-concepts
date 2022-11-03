#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <ucontext.h>
#include <unistd.h>
#include <stdbool.h>

#define THREAD_STACK_SIZE 1024*64
#define MAX_TASKS 32

typedef void (*sut_task_f)();

/**
 * @brief thread scheduler for the compute executor.
 *
 * @return void*
 */
void *cexecScheduler();

/**
 * @brief thread scheduler for the I/O executor. Used to offload the input/output operations
 *        to a separate executor to prevent blocking of the compute executor. It performs the
 *        open, read, write, and close file operations on tasks in the wait queue.
 *        Once those operations are complete, the task is placed back into the task ready queue.
 *
 * @return void*
 */
void *iexecScheduler();

void sut_init();
/**
 * @brief creates a task with a task structure that contains the executing function, the stack,
 *        and the appropriate values filled into the task structure. When task is created, it is
 *        inserted into the task ready queue. The compute execute scheduler is responsible for
 *        pulling the tasks created by this function out of the front of the task ready queue
 *        and executing them.
 *
 * @param fn
 * @return true
 * @return false
 */
bool sut_create(sut_task_f fn);

/**
 * @brief compute executor takes over by first saving the user task's context in a task control block,
 *        placing the task in the back of the task ready queue, and reloading the context of the compute
 *        execute scheduler such that it can pull a task from the front of the queue and start executing it.
 *
 */
void sut_yield();

/**
 * @brief compute executor takes over, but does not save the task's context into a task control block
 *        and does not place the task in the back of the task ready queue. It simply reloads the context
 *        of the compute executor and proceeds with the next task in the task ready queue if there is one.
 *
 */
void sut_exit();

/**
 * @brief compute executor takes over, saves the user task's context in a task control block, and loads the
 *        context of the control executor which then picks the next user task in the ready queue. Meanwhile,
 *        the task asking for I/O open is placed into the wait queue and is picked up by the I/O scheduler which performs
 *        the operation.
 *
 * @param dest
 * @return int
 */
int sut_open(char *dest);

/**
 * @brief compute executor takes over, saves the user task's context into a task control block, and loads the
 *        context of the control executor which then picks the next user task in the ready queue. Meanwhile,
 *        the task asking for I/O write is placed into the wait queue. The I/O scheduler picks up the task and
 *        performs the write operation on the file assuming that a open operation was performed previously and a
 *        file descriptor was stored for this task. The contents of the buf is emptied into the file.
 *
 * @param fd
 * @param buf
 * @param size
 */
void sut_write(int fd, char *buf, int size);

/**
 * @brief compute executor takes over, saves the user task's context into the task control block, and loads the
 *        context of the control executor which then picks the next user task in the task ready queue. Meanwhile,
 *        the task asking for I/O read is placed into the wait queue. The I/O scheduler picks up the task and
 *        performs the read operation from the file with file desriptor fd. Once the data is completely read,
 *        and is available in memory passed into the function by the calling task.
 *
 * @param fd
 * @param buf
 * @param size
 * @return char*
 */
char *sut_read(int fd, char *buf, int size);

/**
 * @brief compute executor takes over, saves the user task's context into the task control block, and loads the
 *        context of the control executor which then picks the next user task in the task ready queue. Meanwhile,
 *        the task asking for I/O close operation is placed into the wait queue. The I/O scheduler picks up the task
 *        performs the close operation on the file with file descriptor fd. The file closing is not done in an asynchronous
 *        manner.
 *
 * @param fd
 */
void sut_close(int fd);

/**
 * @brief the call is responsible for cleanly shutting down the threading library. The main thread that created the
 *        compute executor and I/O executor threads wait for them to terminate before joining them and stopping
 *        the process completely.
 *
 */
void sut_shutdown();
