#include "gipc_thread.h"

typedef void (*gipc_handler)(void* data);

typedef struct gipc_thread_task_t {
    struct gipc_thread_task_t*	next; 		//no need set
    unsigned int        		id; 		//task id, no need set
    void*						ctx; 		//save ctx for handler, user set
    gipc_handler				handler; 	//user set
}gipc_thread_task_t;

typedef struct gipc_thread_pool_t  gipc_thread_pool_t;


gipc_thread_pool_t* gipc_thread_pool_config(unsigned int threads);

gipc_thread_task_t*	gipc_thread_task_alloc(unsigned int size);
int gipc_thread_task_post(gipc_thread_pool_t* tp, gipc_thread_task_t* task);
int gipc_thread_pool_init_worker(gipc_thread_pool_t* tp);
int gipc_thread_pool_exit_worker(gipc_thread_pool_t* tp);

