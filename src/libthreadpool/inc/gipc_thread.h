#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <sched.h>
#include <pthread.h>
#include <limits.h>
#include <sys/syscall.h> 

#if (GIPC_HAVE_SCHED_YIELD)
#define gipc_sched_yield()  sched_yield()
#else
#define gipc_sched_yield()  usleep(1)
#endif

#define GIPC_TID_T_FMT   "%P"

typedef pid_t  					gipc_tid_t;
typedef pthread_mutex_t			gipc_mutex_t; 
typedef pthread_cond_t			gipc_cond_t; 
typedef pthread_mutexattr_t		gipc_mutexattr_t;

#define gipc_mutexattr_init 	pthread_mutexattr_init
#define gipc_mutexattr_settype 	pthread_mutexattr_settype
#define gipc_mutex_init			pthread_mutex_init
#define gipc_mutexattr_destroy 	pthread_mutexattr_destroy
#define gipc_mutex_destroy 		pthread_mutex_destroy
#define gipc_mutex_lock			pthread_mutex_lock
#define gipc_mutex_unlock		pthread_mutex_unlock
#define gipc_cond_init 			pthread_cond_init
#define gipc_cond_destroy		pthread_cond_destroy
#define gipc_cond_signal		pthread_cond_signal
#define gipc_cond_wait 			pthread_cond_wait
#define gipc_attr_init			pthread_attr_init
#define gipc_attr_destroy 		pthread_attr_destroy
#define gipc_attr_setstacksize 	pthread_attr_setstacksize

gipc_tid_t gipc_thread_tid(void);

int gipc_thread_mutex_create(gipc_mutex_t* mtx);
int gipc_thread_mutex_destroy(gipc_mutex_t* mtx);
int gipc_thread_mutex_lock(gipc_mutex_t* mtx);
int gipc_thread_mutex_unlock(gipc_mutex_t* mtx);
int gipc_thread_cond_create(gipc_cond_t* cond);
int gipc_thread_cond_destroy(gipc_cond_t* cond);
int gipc_thread_cond_signal(gipc_cond_t* cond);
int gipc_thread_cond_wait(gipc_cond_t* cond, gipc_mutex_t* mtx);


