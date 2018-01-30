#include "gipc_cache.h"

typedef enum {
	CACHE_BUF_64B,
	CACHE_BUF_128B,
	CACHE_BUF_256B,
	CACHE_BUF_512B,
	CACHE_BUF_1K,
	CACHE_BUF_2K,
	CACHE_BUF_4K,
	CACHE_BUF_MAX,
}CACHE_BUF_SIZE;

typedef int (*cache_cb)(void* data, unsigned int len);

typedef struct cache_pool_node{
	CACHE_NODE		node;
	int				last_use_time;	/*recent use time*/
	union{
		cache_cb	cb;				/*callback called in thread pool*/
		sem_t		ack_sem;		/*semaphore used in queue*/
	}cache_type;
	unsigned short	length;			/*the length of buffer*/
	unsigned short	use_length;		/*use length of buffer*/
	void*			buf;
#define ccb		cache_type.cb
#define ack_sem cache_type.ack_sem
}CACHE_POOL_NODE;


typedef struct 
{
	CACHE_LIST		cacheStack[CACHE_BUF_MAX];
	int				cache_applied_count[CACHE_BUF_MAX];
	pthread_mutex_t	cache_applied_mutex[CACHE_BUF_MAX];
}CACHE_POOL;

int gipc_cache_pool_init(CACHE_POOL* cp);
int gipc_cache_pool_deinit(CACHE_POOL* cp) ;

int cache_pool_empty(CACHE_POOL* cp, int len);
CACHE_POOL_NODE* cache_pool_node_remove(
		CACHE_POOL* cp, unsigned int len);
int cache_pool_node_recycle(CACHE_POOL* cp, CACHE_POOL_NODE* node);
int cache_pool_print_count(CACHE_POOL* cp);
void cache_pool_node_manage(CACHE_POOL* cp);



