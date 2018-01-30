
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "utils.h"
#include "gipc_message.h"

typedef struct cache_node {
  struct cache_node *next; /**< Next element in the list */
  struct cache_node *prev; /**< Previous element in the list */
}CACHE_NODE;

typedef struct cache_list {
	CACHE_NODE* 	head;
	CACHE_NODE* 	tail;
	unsigned int 	count;
	sem_t			list_sem;
	pthread_mutex_t	list_mutex;
}CACHE_LIST;

int cache_list_init(CACHE_LIST* list);
int cache_list_empty(CACHE_LIST* list);
int cache_list_add_head(CACHE_LIST* list, CACHE_NODE* n);
int cache_list_add_tail(CACHE_LIST* list, CACHE_NODE* n);
CACHE_NODE* cache_list_remove_head(CACHE_LIST* list, int wait);
CACHE_NODE* cache_list_remove_tail(CACHE_LIST* list, int wait);
CACHE_NODE* cache_list_remove_node(CACHE_LIST* list, CACHE_NODE* node);




