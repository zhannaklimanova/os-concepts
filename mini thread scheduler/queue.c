#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include "queue.h"
#include "sut.h"

struct queue queue_create() {
    struct queue q = STAILQ_HEAD_INITIALIZER(q);
    return q;
}

void queue_init(struct queue *q) {
    STAILQ_INIT(q);
}

void queue_error() {
    fprintf(stderr, "Fatal error in queue operations\n");
    exit(1);
}

void queue_insert_head(struct queue *q, struct queue_entry *e) {
    STAILQ_INSERT_HEAD(q, e, entries);
}

void queue_insert_tail(struct queue *q, struct queue_entry *e) {
    STAILQ_INSERT_TAIL(q, e, entries);
}

struct queue_entry *queue_peek_front(struct queue *q) {
    return STAILQ_FIRST(q);
}

struct queue_entry *queue_pop_head(struct queue *q) {
    struct queue_entry *elem = queue_peek_front(q);
    if(elem) {
        STAILQ_REMOVE_HEAD(q, entries);
    }
    return elem;
}