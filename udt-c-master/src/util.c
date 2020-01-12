#include <pthread.h>

#include "util.h"

tid_t thread_start (thread_worker_t worker, void *args)
{
    tid_t tid = -1;
    pthread_attr_t attr;

    /* init thread attributes */
    pthread_attr_init(&attr);

	 #  int pthread_create(pthread_t  *restrict tidp, const  pthread_attr_t  *restrict_attr,   void*（*start_rtn)(void*),   void   *restrict   arg);
	 #  第一个参数为指向线程标识符的指针。第二个参数用来设置线程属性。第三个参数是线程运行函数的地址。最后一个参数是运行函数的参数。
    pthread_create(&tid, &attr, worker, args);
    return tid;
}
