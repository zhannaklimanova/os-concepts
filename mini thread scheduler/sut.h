// #ifndef __SUT_H__
// #define __SUT_H__

// #include <stdbool.h>

// #include <ucontext.h>

// #include <errno.h>
// #include <stdio.h>
// #include <stdint.h>
// #include <stdlib.h>
// #include <string.h>
// #include <arpa/inet.h>
// #include <sys/types.h>
// #include <sys/socket.h>

// #include <unistd.h>
// #include <sys/wait.h>

// #include "queue.h"

// // CONSTANTS
// #define MAX_THREADS                        32
// #define THREAD_STACK_SIZE                  1024*64
// #define BUFSIZE 4096

// // Task definition
// typedef void (*sut_task_f)();

// // Thread descriptor structure
// typedef struct __threaddesc {
//   int threadid;
//   char* threadstack;
//   sut_task_f* threadfunc; // void* threadfunc;
//   ucontext_t threadcontext;
// } threaddesc;


// extern int numthreads;

// /*--- Kernel level threads ---*/
// void *c_exec();
// void *i_exec();

// /*--- SUT library ---*/
// void sut_init();
// bool sut_create(sut_task_f fn);
// void sut_yield();
// void sut_exit();
// int sut_open(char *dest);
// // void sut_write(char *buf, int size);
// // void sut_close();
// // char *sut_read();
// void sut_shutdown();

// #endif

