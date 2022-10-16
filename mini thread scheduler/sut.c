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
#include <pthread.h>
#include <assert.h>

/* -- Definitions -- */
void sut_init();

void cexec_initializer();

// C-EXEC kernel thread
pthread_t *cexec;

// I-EXEC kernel thread
pthread_t *iexec;

// Create C-EXEC Mutex lock
pthread_mutex_t cexec_lock;

// Create I/O Mutex lock
pthread_mutex_t iexec_lock;

void sut_init() 
{
    int rc;

    // Allocated memory for C-EXEC thread
    cexec = (pthread_t *)malloc(sizeof(pthread_t));

    // Allocate memory for I-EXEC thread
    iexec = (pthread_t *)malloc(sizeof(pthread_t));

    // Initialize mutex locks to their default values
    rc = pthread_mutex_init(&cexec_lock, PTHREAD_MUTEX_DEFAULT);
    assert(rc == 0);
    rc = pthread_mutex_init(&iexec_lock, PTHREAD_MUTEX_DEFAULT);
    assert(rc == 0);

    // Create C-EXEC kernel thread
    pthread_create(&cexec, NULL, cexec_initializer, NULL);

    // Create I-EXEC kernel thread TODO
    pthread_create(&iexec, NULL, iexec_initializer, NULL);
    
}

void cexec_initializer()
{
    return;
}

void iexec_initializer()
{
    return;
}

