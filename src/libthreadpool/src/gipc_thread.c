#include "gipc_thread.h"



#if (GIPC_LINUX)

/*
 * Linux thread id is a pid of thread created by clone(2),
 * glibc does not provide a wrapper for gettid().
 */
gipc_tid_t gipc_thread_tid(void) {
    return syscall(SYS_gettid);
}

#else
gipc_tid_t gipc_thread_tid(void) {
    return  pthread_self();
}
#endif


int gipc_thread_mutex_create(gipc_mutex_t* mtx) {
	int	err = -1;
    gipc_mutexattr_t  attr;

    if (gipc_mutexattr_init(&attr) != 0) {
        return -1;
    }
    if (gipc_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK) != 0) {
        printf("pthread_mutexattr_settype() failed\n");
        return -1;
    }
    if (gipc_mutex_init(mtx, &attr) != 0) {
        printf("pthread_mutex_init() failed\n");
        return -1;
    }
    if (gipc_mutexattr_destroy(&attr) != 0) {
        printf("pthread_mutexattr_destroy() failed");
    }
    //printf("pthread_mutex_init(%p)\n", mtx);
    return 0;
}

int gipc_thread_mutex_destroy(gipc_mutex_t* mtx) {

    if (gipc_mutex_destroy(mtx) != 0) {
        printf("pthread_mutex_destroy() failed\n");
        return -1;
    }
   // printf("pthread_mutex_destroy(%p)\n", mtx);
    return 0;
}
int gipc_thread_mutex_lock(gipc_mutex_t* mtx) {
	if (gipc_mutex_lock(mtx) != 0) {
		return -1;
	}
	//printf("gipc_mutex_lock(%p)\n", mtx);
	return 0;
}
int gipc_thread_mutex_unlock(gipc_mutex_t* mtx) {
	if (gipc_mutex_unlock(mtx) != 0) {
		return -1;
	}
	//printf("gipc_mutex_unlock(%p)\n", mtx);
	return 0;
}

int gipc_thread_cond_create(gipc_cond_t* cond) {
	
	if (gipc_cond_init(cond, NULL) != 0) {
		return -1;
	}
	//printf("gipc_cond_init(%p)\n", cond);
	return 0;

}
int gipc_thread_cond_destroy(gipc_cond_t* cond) {
	if (gipc_cond_destroy(cond) != 0) {
		return -1;
	}
	//printf("gipc_cond_destroy(%p)\n", cond);
	return 0;
}
int gipc_thread_cond_signal(gipc_cond_t* cond) {
	if (gipc_cond_signal(cond) != 0) {
		return -1;
	}
	//printf("gipc_cond_signal(%p)\n", cond);
	return 0;
}
int gipc_thread_cond_wait(gipc_cond_t* cond, gipc_mutex_t* mtx) {
	if (gipc_cond_wait(cond, mtx) != 0) {
		return -1;
	}
	//printf("gipc_cond_wait(%p)\n", cond);
	return 0;
}



