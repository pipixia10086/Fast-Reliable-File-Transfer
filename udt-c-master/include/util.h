#ifndef UTIL_H_RSWIA2KL
#define UTIL_H_RSWIA2KL

#include <pthread.h>
#include "config.h"


#ifdef DEBUG

#include <stdio.h>
#define console_log_mod(MODIFIER, LOGDATA)  fprintf(stderr, MODIFIER, LOGDATA)
#define console_log(LOGDATA)                fprintf(stderr, "%s\n", LOGDATA)

#else

#define console_log_mod(MODIFIER, LOGDATA)
#define console_log(LOGDATA)

#endif  /* end of DEBUG */


#define linked_list_add(BUFFER, BLOCK) \
{ \
# 当pthread_mutex_lock()返回时，该互斥锁已被锁定。
# 线程调用该函数让互斥锁上锁，如果该互斥锁已被另一个线程锁定和拥有，
# 则调用该线程将阻塞，直到该互斥锁变为可用为止。  BUFFER.mutex 存的就是 初始化buffer时定义的锁
    pthread_mutex_lock(&(BUFFER.mutex)); \
	
    if (BUFFER.size == 0) { \
        BUFFER.first = BLOCK; \
    } else { \
        BLOCK -> next = BUFFER.last; \
        BLOCK -> next -> next = BLOCK; /* BUFFER.last -> lext = BLOCK */ \
        BLOCK -> next = NULL; \
    } \
    BUFFER.last = BLOCK; \
    BUFFER.size++; \
    pthread_mutex_unlock(&(BUFFER.mutex)); \
}

#define linked_list_get(BUFFER, BLOCK) \
{ \
# 取出链表中的第一个。
    pthread_mutex_lock(&(BUFFER.mutex)); \
    if (BUFFER.size == 0) { \
        BLOCK = NULL; \
    } else { \
        BLOCK = BUFFER.first; \
        BUFFER.first = BLOCK -> next; \
        BUFFER.size--; \
    } \
    pthread_mutex_unlock(&(BUFFER.mutex)); \
}

typedef pthread_t tid_t;
typedef void * (*thread_worker_t) (void *);

tid_t thread_start (thread_worker_t, void *); 
void  thread_stop  (tid_t);

#endif /* end of include guard: UTIL_H_RSWIA2KL */
