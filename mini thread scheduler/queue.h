#pragma once // compiler directive in place of #ifndef and #define

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include "sut.h"

struct queue_entry {
    char stack[THREAD_STACK_SIZE];
    ucontext_t context;
    bool isTaskActive;
    STAILQ_ENTRY(queue_entry) entries;
    struct ioOperation {
        enum ioOperationType {
            NO_IO_OP_REQUESTED = 0,
            OPEN_OP = 1,
            READ_OP = 2,
            WRITE_OP = 3,
            CLOSE_OP = 4
        } ioOperationType;
        struct ioOperationData {
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

void queue_insert_head(struct queue *q, struct queue_entry *e);

void queue_insert_tail(struct queue *q, struct queue_entry *e);

struct queue_entry *queue_peek_front(struct queue *q);

struct queue_entry *queue_pop_head(struct queue *q);

