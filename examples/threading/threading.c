#define _GNU_SOURCE // use the gnu extension so we have pthread_tryjoin_mp available
#include "threading.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    int err = 0;
    struct thread_data* td = thread_param;
    DEBUG_LOG("sleeping for: %dms", td->wait_to_obtain_ms);
    usleep(td->wait_to_obtain_ms * 1000);
    DEBUG_LOG("Done sleeping...");
    if((err = pthread_mutex_lock(td->mutex))) {
        ERROR_LOG("Unable to obtain mutex: %s", strerror(err));
        td->thread_complete_success = false;
        return thread_param;
    } else {
        DEBUG_LOG("Lock acquired!");
    }
    DEBUG_LOG("sleeping for: %dms", td->wait_to_obtain_ms);
    usleep(td->wait_to_release_ms * 1000);
    DEBUG_LOG("Done sleeping...");

    if((err = pthread_mutex_unlock(td->mutex))) {
        ERROR_LOG("Unable to unlock mutex: %s", strerror(err));
        td->thread_complete_success = false;
        return thread_param;
    } else {
        DEBUG_LOG("Unlocked mutex!");
    }

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    td->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    struct thread_data* td = malloc(sizeof(struct thread_data));
    int err = 0;
    td->mutex = mutex;
    td->wait_to_obtain_ms = wait_to_obtain_ms;
    td->wait_to_release_ms = wait_to_release_ms;

    if((err = pthread_create(thread, NULL, threadfunc, td))) {
        ERROR_LOG("Unable to start thread: %s", strerror(err));
        return false;
    } else {
        DEBUG_LOG("Thread created successful!");
    }

    return true;
}

