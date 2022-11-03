#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include "sut2.h"

// struct queue_entry {
//     void *data;
//     STAILQ_ENTRY(queue_entry) entries;
// };

// STAILQ_HEAD(queue, queue_entry);

struct queue_entry {
    char stack[THREAD_STACK_SIZE];
    ucontext_t context;
    bool isTaskActive;
    STAILQ_ENTRY(queue_entry) entries;
    void *data; // rm
    struct ioOperation {
        enum ioOperationType {
            NO_IO_OP_REQUESTED = 0,
            OPEN_OP = 1,
            READ_OP = 2,
            WRITE_OP = 3,
            CLOSE_OP = 4
        } ioOperationType;
        struct ioOperationData { // Only one of these members can be set at any given time
            struct openFile {
                char *fname;
                int returnVal;
            } openFile;
            struct readFile {
                int fd;
                char *buf;
                int size;
                char *returnVal;
            } readFile;
            struct writeFile {
                int fd;
                char *buf;
                int size;
            } writeFile;
            struct closeFile {
                int fd;
            } closeFile;
        } ioOperationData;
    } ioOperation;
};

STAILQ_HEAD(queue, queue_entry);

struct queue queue_create();

void queue_init(struct queue *q);

void queue_error();

struct queue_entry *queue_new_node(void *data);

void queue_insert_head(struct queue *q, struct queue_entry *e);

void queue_insert_tail(struct queue *q, struct queue_entry *e);

struct queue_entry *queue_peek_front(struct queue *q);

struct queue_entry *queue_pop_head(struct queue *q);

#endif

