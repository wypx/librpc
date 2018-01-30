#include "gipc_cache.h"

                   
int cache_list_init(CACHE_LIST* list) {
	if( !list ) {
		return -1;
	}
	pthread_mutex_init(&list->list_mutex, NULL);
	pthread_mutex_lock(&list->list_mutex);
	
	list->head 	= NULL;
	list->tail 	= NULL;
	list->count = 0;
	sem_init(&list->list_sem, 0, 0);
	pthread_mutex_unlock(&list->list_mutex);

	return 0;
}
int cache_list_empty(CACHE_LIST* list) {
	return (list->head == NULL);
}
int cache_list_add_head(CACHE_LIST* list, CACHE_NODE* n) {
	if(!list || !n)
		return -1;

	pthread_mutex_lock(&list->list_mutex);
	if(cache_list_empty(list)) {
		list->tail = list->head = n;
		list->head->next = list->head->prev = NULL;
	} else {
		list->tail->next = n;
		n->prev = list->tail;
		n->next = NULL;
		list->tail = n;
	}
	list->count++;
	
	pthread_mutex_unlock(&list->list_mutex);	
	sem_post(&list->list_sem);

	return 0;
}
int cache_list_add_tail(CACHE_LIST* list, CACHE_NODE* n) {
	if(!list || !n)
		return -1;
	
	pthread_mutex_lock(&list->list_mutex);

	if(cache_list_empty(list)) {
		list->tail = list->head = n;
		list->head->next = list->head->prev = NULL;	
	} else {
		n->prev = NULL;
		n->next = list->head;
		list->head->prev = n;
		list->head = n;
	}
	list->count++;

	pthread_mutex_unlock(&list->list_mutex);
	sem_post(&list->list_sem);

	return 0;
}
CACHE_NODE* cache_list_remove_head(CACHE_LIST* list, int wait) {
	CACHE_NODE* n;

	if(!list)
		return NULL;

	sem_wait_i(&list->list_sem, wait);	
	
	pthread_mutex_lock(&list->list_mutex);
	if(!list->head) {
		pthread_mutex_unlock(&list->list_mutex);
		return NULL;
	} else {
		n = list->head;	
		if(list->head == list->tail)
			list->head = list->tail = NULL;
		else
		{
			list->head = list->head->next;	
			list->head->prev = NULL;	
		}
	}
	list->count--;
	pthread_mutex_unlock(&list->list_mutex);

	n->next = n->prev = NULL;

	return n;
}
CACHE_NODE* cache_list_remove_tail(CACHE_LIST* list, int wait) {
	CACHE_NODE* n;

	if(!list)
		return NULL;

	sem_wait_i(&list->list_sem, wait);		

	pthread_mutex_lock(&list->list_mutex);
	if(!list->tail)
	{
		pthread_mutex_unlock(&list->list_mutex);
		return NULL;
	}
	else
	{
		n = list->tail;	
		if(list->head == list->tail)
			list->head = list->tail = NULL;
		else
		{
			list->tail = list->tail->prev;
			list->tail->next = NULL;
		}
	}
	list->count--;
	pthread_mutex_unlock(&list->list_mutex);

	n->next = n->prev = NULL;

	return n;
}
CACHE_NODE* cache_list_remove_node(CACHE_LIST* list, CACHE_NODE* n) {
	CACHE_NODE* node = NULL;

	node = list->head;	

	while(node)
	{
		if(node == n)
			break;
		
		node = node->next;
	}

	if(node)
	{
		pthread_mutex_unlock(&list->list_mutex);

		if(list->head == list->tail)
		{
			list->head = list->tail = NULL;
		}
		else if(list->head == node)
		{
			list->head = list->head->next;
			list->head->prev = NULL;
		}
		else if(list->tail == node)
		{
			list->tail = node->prev;
			list->tail->next = NULL;
		}
		else
		{
			node->prev->next = node->next;
			node->next->prev = node->prev;
			node->prev = node->next = NULL;
		}

		list->count--;
		pthread_mutex_unlock(&list->list_mutex);
	}

	return node;
}


