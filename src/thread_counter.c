#include "thread_counter.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>



struct thread_counter {
    int c;
} ;

pthread_mutex_t mutex;


/*
 * Initialize a new thread counter.
 */
THREAD_COUNTER *tcnt_init(){
    pthread_mutex_init(&mutex, NULL);

    return malloc(sizeof(int));
}
/*
 * Finalize a thread counter.
 */
void tcnt_fini(THREAD_COUNTER *tc){
    free(tc);
    pthread_mutex_destroy(&mutex);


}
/*
 * Increment a thread counter.
 */
void tcnt_incr(THREAD_COUNTER *tc){
    pthread_mutex_lock(&mutex);
    tc->c = tc->c + 1;

    pthread_mutex_unlock(&mutex);
}

/*
 * Decrement a thread counter, alerting anybody waiting
 * if the thread count has dropped to zero.
 */
void tcnt_decr(THREAD_COUNTER *tc){
    pthread_mutex_lock(&mutex);
    tc->c = tc->c - 1;

    pthread_mutex_unlock(&mutex);
}

/*
 * A thread calling this function will block in the call until
 * the thread count has reached zero, at which point the
 * function will return.
 */
void tcnt_wait_for_zero(THREAD_COUNTER *tc){

    while(tc->c != 0){
        pthread_mutex_lock(&mutex);
        if (tc->c == 0){
            pthread_mutex_unlock(&mutex);
            break;
        }
        else{
            pthread_mutex_unlock(&mutex);
        }
    }
}

