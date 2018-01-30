#include "gipc_cache_pool.h"

#include "common.h"
#include <sys/sysinfo.h>


#define CACHE_DEFAULT_EN_USE_TIME	(120)	/*use time of event node*/


/*every block size in event pool*/
static const int cache_buf_len[CACHE_BUF_MAX] = 
	{ 64, 128, 256, 512, 1024, 2048, 4096	};

static int system_uptime();
static CACHE_POOL_NODE* cache_pool_node_create(
				CACHE_POOL* cp, CACHE_BUF_SIZE cache_buf_index);

static int system_uptime()
{
	unsigned int uptime;
	struct sysinfo info;
	memset(&info, 0, sizeof(info));
	(void)sysinfo(&info);
	uptime = info.uptime;
	return uptime;
}

int gipc_cache_pool_init(CACHE_POOL* cp) {
	
	CACHE_BUF_SIZE index = 0;
	memset(cp, 0, sizeof(CACHE_POOL));

	for(index = 0; index < CACHE_BUF_MAX; index++) {
		pthread_mutex_init(&cp->cache_applied_mutex[index], NULL);
	}
	return 0;
}

int gipc_cache_pool_deinit(CACHE_POOL* cp) {
	return 0;
}

int cache_pool_empty(CACHE_POOL* cp, int len) {
	int index = 0;
	for(index = 0; index < CACHE_BUF_MAX; index++) {
		if(cache_buf_len[index] == len) {
			return cache_list_empty(&cp->cacheStack[index]);
		}
	}
	return 0;
}

static CACHE_POOL_NODE* cache_pool_node_create(
	CACHE_POOL* cp, CACHE_BUF_SIZE cache_buf_index) {
	
	CACHE_POOL_NODE* node = NULL;
	int len = cache_buf_len[cache_buf_index];

	if(!cp)
		return NULL;

	node = (CACHE_POOL_NODE*)malloc(sizeof(CACHE_POOL_NODE));
	if(node) {
		memset(node, 0, sizeof(CACHE_POOL_NODE));
		node->buf = malloc(len);
		if(node->buf)
		{
			memset(node->buf, 0, len);
			node->length = len;
			pthread_mutex_lock(&cp->cache_applied_mutex[cache_buf_index]);
			cp->cache_applied_count[cache_buf_index]++;
			pthread_mutex_unlock(&cp->cache_applied_mutex[cache_buf_index]);
			
			printf("cache_node_create, malloc success, len:[%d]\n", 
					cache_buf_len[cache_buf_index]);
		}
		else
		{
			printf("cache_pool_node_create unix_malloc node->buf failed\n");
			sfree(node);	
		}
	}
	else
		printf("cache_pool_node_create unix_malloc node failed\n");
	
	return node;
}

CACHE_POOL_NODE* cache_pool_node_remove(
		CACHE_POOL* cp, unsigned int len) {
		
	
	int cache_buf_index = 0;
	CACHE_POOL_NODE * node = NULL;

	if(0 == len)
		return NULL;
	else if (len <= 64)
		cache_buf_index = CACHE_BUF_64B;
	else if (len <= 128)
		cache_buf_index = CACHE_BUF_128B;
	else if (len <= 256)
		cache_buf_index = CACHE_BUF_256B;
	else if (len <= 512)
		cache_buf_index = CACHE_BUF_512B;
	else if (len <= 1024)
		cache_buf_index = CACHE_BUF_1K;
	else if (len <= 2048) 
		cache_buf_index = CACHE_BUF_2K;
	else if (len <= 4096)
		cache_buf_index = CACHE_BUF_4K;
	else
		printf("cache_pool_node_remove len too large\n");

	if(cache_list_empty(&cp->cacheStack[cache_buf_index])
			&& cp->cache_applied_count[cache_buf_index] < 80) {
		node = cache_pool_node_create(cp, cache_buf_index);				
	} else {
		if(cp->cache_applied_count[cache_buf_index] >= 80)
			
			printf("cache_pool_node_remove, \
					length:[%d] pool applied 80 node\n",
					cp->cache_applied_count[cache_buf_index]);
		
		node = (CACHE_POOL_NODE*)cache_list_remove_head(
			&cp->cacheStack[cache_buf_index], GIPC_NO_WAIT);
		if(!node)
			printf("cache_pool_node_remove node NULL\n");
	}

	return node;
}
int cache_pool_node_recycle(CACHE_POOL* cp, CACHE_POOL_NODE* node) {
	
	int index = 0;
	
	if(!node) {
		return -1;
	}

	node->last_use_time = system_uptime();
	
	memset(node->buf, 0, node->length);

	for(index = 0; index < CACHE_BUF_MAX; index++) {
		if(cache_buf_len[index] == node->length) {
			cache_list_add_head(&cp->cacheStack[index], (CACHE_NODE*)node);
			break;
		}
	}

	if(CACHE_BUF_MAX == index) {
		printf("cache_pool_node_recycle, error length:[%d] of buffer\n",
			node->length);
		return -1;
	}
	
	return 0;
}

int cache_pool_print_count(CACHE_POOL* cp) {
	int index = 0;
	for(index = 0; index < CACHE_BUF_MAX; index++)
		printf("cache_pool length:[%d], count:[%d], applied count:[%d]\n", 
				cache_buf_len[index], 
				(cp->cacheStack[index]).count, 
				cp->cache_applied_count[index]);
	return 0;
}

static int cache_pool_node_destroy(CACHE_POOL* cp, 
		CACHE_POOL_NODE* node, CACHE_BUF_SIZE ep_buf_index)
{
	if(!cp)
		return -1;

	if(node) {
		sfree(node->buf);
		sfree(node);
		pthread_mutex_lock(&cp->cache_applied_mutex[ep_buf_index]);
		cp->cache_applied_count[ep_buf_index]--;
		pthread_mutex_unlock(&cp->cache_applied_mutex[ep_buf_index]);
	}
	return 0;
}

void cache_pool_node_manage(CACHE_POOL* cp) {
	
	CACHE_POOL_NODE* node = NULL;
	int uptime = 0;
	int index = 0;
	
	if(!cp)
		return;

	for(index = 0; index < CACHE_BUF_MAX; index++)
	{
		uptime = system_uptime();

		while((node = (CACHE_POOL_NODE*)(cp->cacheStack[index]).tail))
		{
			/*If current node long time has not be used, we will recycle the memery*/
			if((uptime - node->last_use_time) > CACHE_DEFAULT_EN_USE_TIME)
			{
				printf("cache_pool_node_manage, \
						a node that long time unused, length:[%d]\n", 
						cache_buf_len[index]);
				node = (CACHE_POOL_NODE*)cache_list_remove_tail(&cp->cacheStack[index], GIPC_NO_WAIT);
				if(node)
					cache_pool_node_destroy(cp, node, index);
				else
					printf("cache_pool_node_manage don't unix_free\n");
			}
			else
			{
				/* Tail is the oldest node in the queue, if tail node is used
				 * recently, so the other node is also used recently .*/
				break;
			}
		}
	}
	
}

