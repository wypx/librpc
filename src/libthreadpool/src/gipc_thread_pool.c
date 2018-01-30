#include "common.h"
#include "gipc_thread_pool.h"

typedef struct {
    gipc_thread_task_t*		first;
    gipc_thread_task_t**	last;
} gipc_thread_pool_queue_t;

#define gipc_thread_pool_queue_init(q)  \
    (q)->first = NULL;                  \
    (q)->last = &(q)->first


typedef struct gipc_thread_pool_t {
    gipc_mutex_t        		mtx;
    gipc_thread_pool_queue_t   	queue;
    int                 	  	waiting;
    gipc_cond_t         		cond;

    char*                 		name;
    unsigned int            	threads;
	int                 		max_queue;
}gipc_thread_pool_t;

static int 	gipc_thread_pool_init(gipc_thread_pool_t *tp);
static void gipc_thread_pool_destroy(gipc_thread_pool_t *tp);
static void gipc_thread_pool_exit_handler(void *data);

static void* gipc_thread_pool_cycle(void *data);
static void  gipc_thread_pool_handler();

static unsigned int  gipc_thread_pool_task_id;
//static gipc_atomic_t         		gipc_thread_pool_done_lock;
//static gipc_thread_pool_queue_t  	gipc_thread_pool_done;

static gipc_thread_pool_t g_tp; 
int   gipc_ncpu = 1;

static int gipc_thread_pool_init(gipc_thread_pool_t *tp)
{
    pthread_t       tid = ~0;
    unsigned int    n   =  0;
    pthread_attr_t  attr;

    gipc_thread_pool_queue_init(&tp->queue);

    if (gipc_thread_mutex_create(&tp->mtx) != 0) {
        return -1;
    }

    if (gipc_thread_cond_create(&tp->cond) != 0) {
        (void)gipc_thread_mutex_destroy(&tp->mtx);
        return -1;
    }
	
    if (gipc_attr_init(&attr) != 0) {
        printf("pthread_attr_init() failed\n");
        return -1;
    }

    if (gipc_attr_setstacksize(&attr, PTHREAD_STACK_MIN) != 0) {
        printf("pthread_attr_setstacksize() failed");
        return -1;
    }


    for (n = 0; n < tp->threads; n++) {
        if (pthread_create(&tid, &attr, gipc_thread_pool_cycle, tp) != 0) {
            printf("pthread_create() failed\n");
            return -1;
        }
    }

    (void)gipc_attr_destroy(&attr);

    return 0;
}


static void gipc_thread_pool_destroy(gipc_thread_pool_t *tp)
{
    unsigned int           	n;
    gipc_thread_task_t    	task;
    volatile unsigned int   lock;

    gipc_memzero(&task, sizeof(gipc_thread_task_t));

    task.handler = gipc_thread_pool_exit_handler;
    task.ctx = (void *) &lock;

    for (n = 0; n < tp->threads; n++) {
        lock = 1;
        if (gipc_thread_task_post(tp, &task) != 0) {
            return;
        }
        while (lock) {
            gipc_sched_yield();
        }
        //task.event.active = 0;
    }

    (void)gipc_thread_cond_destroy(&tp->cond);

    (void)gipc_thread_mutex_destroy(&tp->mtx);
}


static void gipc_thread_pool_exit_handler(void *data)
{
    unsigned int *lock = data;

    *lock = 0;

    pthread_exit(0);
}


gipc_thread_task_t* gipc_thread_task_alloc(unsigned int size)
{
    gipc_thread_task_t  *task;

   // task = ngx_pcalloc(pool, sizeof(ngx_thread_task_t) + size);
    task = malloc(sizeof(gipc_thread_task_t) + size);
    if (task == NULL) {
        return NULL;
    }

    task->ctx = task + 1;

    return task;
}


int gipc_thread_task_post(gipc_thread_pool_t* tp, gipc_thread_task_t* task)
{
    //if (task->event.active) {
        //ngx_log_error(NGX_LOG_ALERT, tp->log, 0,
        //              "task #%ui already active", task->id);
        //return NGX_ERROR;
    //}

    if (gipc_thread_mutex_lock(&tp->mtx) != 0) {
        return -1;
    }

    if (tp->waiting >= tp->max_queue) {
        (void)gipc_thread_mutex_unlock(&tp->mtx);

        //printf("thread pool \"%V\" queue overflow: %i tasks waiting",
		//		tp->name, tp->waiting);
        return -1;
    }

    //task->event.active = 1;

    task->id = gipc_thread_pool_task_id++;
    task->next = NULL;

    if (gipc_thread_cond_signal(&tp->cond) != 0) {
        (void)gipc_thread_mutex_unlock(&tp->mtx);
        return -1;
    }

    *tp->queue.last = task;
    tp->queue.last = &task->next;

    tp->waiting++;

    (void)gipc_thread_mutex_unlock(&tp->mtx);

    //ngx_log_debug2(NGX_LOG_DEBUG_CORE, tp->log, 0,
    //               "task #%ui added to thread pool \"%V\"",
    //               task->id, &tp->name);

    return 0;
}


static void* gipc_thread_pool_cycle(void *data)
{
    gipc_thread_pool_t *tp = data;

    int                 err;
    sigset_t            set;
    gipc_thread_task_t* task;

#if 0
    ngx_time_update();
#endif

    //ngx_log_debug1(NGX_LOG_DEBUG_CORE, tp->log, 0,
    //               "thread in pool \"%V\" started", &tp->name);

    sigfillset(&set);

    sigdelset(&set, SIGILL);
    sigdelset(&set, SIGFPE);
    sigdelset(&set, SIGSEGV);
    sigdelset(&set, SIGBUS);

    err = pthread_sigmask(SIG_BLOCK, &set, NULL);
    if (err) {
        printf("pthread_sigmask() failed");
        return NULL;
    }

    for ( ;; ) {
        if (gipc_thread_mutex_lock(&tp->mtx) != 0) {
            return NULL;
        }

        /* the number may become negative */
        tp->waiting--;

        while (tp->queue.first == NULL) {
            if (gipc_thread_cond_wait(&tp->cond, &tp->mtx) != 0)
            {
                (void)gipc_thread_mutex_unlock(&tp->mtx);
                return NULL;
            }
        }

        task = tp->queue.first;
        tp->queue.first = task->next;

        if (tp->queue.first == NULL) {
            tp->queue.last = &tp->queue.first;
        }

        if (gipc_thread_mutex_unlock(&tp->mtx) != 0) {
            return NULL;
        }

#if 0
        ngx_time_update();
#endif

        //ngx_log_debug2(NGX_LOG_DEBUG_CORE, tp->log, 0,
        //               "run task #%ui in thread pool \"%V\"",
        //               task->id, &tp->name);

        task->handler(task->ctx);

        //ngx_log_debug2(NGX_LOG_DEBUG_CORE, tp->log, 0,
        //               "complete task #%ui in thread pool \"%V\"",
        //               task->id, &tp->name);

        task->next = NULL;
        
        //ngx_spinlock(&ngx_thread_pool_done_lock, 1, 2048);
        //
        //*ngx_thread_pool_done.last = task;
        //ngx_thread_pool_done.last = &task->next;
        //
        //ngx_memory_barrier();
        //
        //ngx_unlock(&ngx_thread_pool_done_lock);

        //this is nginx call(notify) event after the task is done
        //(void) ngx_notify(ngx_thread_pool_handler);
    }
}

#if 0
static void gipc_thread_pool_handler()
{
    gipc_event_t        *event;
    gipc_thread_task_t  *task;

    printf("thread pool handler");

    gipc_spinlock(&ngx_thread_pool_done_lock, 1, 2048);
    task = ngx_thread_pool_done.first;
    gipc_thread_pool_done.first = NULL;
    gipc_thread_pool_done.last = &ngx_thread_pool_done.first;

    gipc_memory_barrier();

    gipc_unlock(&ngx_thread_pool_done_lock);

    while (task) {
        //ngx_log_debug1(NGX_LOG_DEBUG_CORE, ev->log, 0,
        //               "run completion handler for task #%ui", task->id);

        event = &task->event;
        task = task->next;

        event->complete = 1;
        event->active = 0;

        event->handler(event);
    }
}
#endif
gipc_thread_pool_t*  gipc_thread_pool_config(unsigned int threads)
{
    if (threads < 1) {
        threads = 1;
    }
    
    gipc_thread_pool_t* tp = &g_tp;
    
    if ( tp->threads ) {
        return tp;
    }
    
    tp->threads = threads;
    tp->max_queue = 16;
    
    return tp;
}

int gipc_thread_pool_init_worker(gipc_thread_pool_t* tp)
{
    //ngx_uint_t                i;
    //ngx_thread_pool_t       **tpp;
    //ngx_thread_pool_conf_t   *tcf;

    //if (ngx_process != NGX_PROCESS_WORKER
    //    && ngx_process != NGX_PROCESS_SINGLE)
    //{
    //    return NGX_OK;
    //}

    //tcf = (ngx_thread_pool_conf_t *) ngx_get_conf(cycle->conf_ctx,
    //                                              ngx_thread_pool_module);
    //
    //if (tcf == NULL) {
    //    return NGX_OK;
    //}
    //
    //ngx_thread_pool_queue_init(&ngx_thread_pool_done);
    //
    //tpp = tcf->pools.elts;
    //
    //for (i = 0; i < tcf->pools.nelts; i++) {
    //    if (ngx_thread_pool_init(tpp[i], cycle->log, cycle->pool) != NGX_OK) {
    //        return NGX_ERROR;
    //    }
    //}
    
    gipc_ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    printf("ncpu %d \n", gipc_ncpu);
    
    if (gipc_thread_pool_init(tp) != 0) {
        return -1;
    }

    return 0;
}


int gipc_thread_pool_exit_worker(gipc_thread_pool_t* tp) {
    //ngx_uint_t                i;
    //ngx_thread_pool_t       **tpp;
    //ngx_thread_pool_conf_t   *tcf;

    //if (ngx_process != NGX_PROCESS_WORKER
    //    && ngx_process != NGX_PROCESS_SINGLE)
    //{
    //    return;
    //}
    //
    //tcf = (ngx_thread_pool_conf_t *) ngx_get_conf(cycle->conf_ctx,
    //                                              ngx_thread_pool_module);
    //
    //if (tcf == NULL) {
    //    return;
    //}
    //
    //tpp = tcf->pools.elts;
    //
    //for (i = 0; i < tcf->pools.nelts; i++) {
    //    ngx_thread_pool_destroy(tpp[i]);
    //}
    
    gipc_thread_pool_destroy(tp);
	return 0;
}


