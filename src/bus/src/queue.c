#include "queue.h"

void queue_push(struct queue_item* fitem, struct queue_item* nitem) {
    if (fitem->next!= NULL) {
        struct queue_item* ptr = fitem->next;

        while(ptr->next != NULL) {
            ptr = ptr->next;
        }

        ptr->next = nitem;
    } else {
    	fitem->next = nitem;
    }
}
struct queue_item* queue_pop(struct queue_item* fitem) {
	if ( !fitem->next ) {
		return NULL;
	} else {
	   	struct queue_item* pResult = fitem->next;
	   	fitem->next = fitem->next->next;
	   	return pResult;
	}	
}
struct queue_item* queue_create_item(char* packet, 
		unsigned int length, unsigned int srcid, unsigned int destid)	{
		
	struct queue_item* nitem = (struct queue_item*)malloc(sizeof(struct queue_item));
	if( !nitem )
		return NULL;
	
	memcpy(nitem->packet, packet, length);
	nitem->length 	= length;
	nitem->srcid  	= srcid;
	nitem->destid 	= destid;
	nitem->next 	= NULL;
	
	return nitem;
}
int queue_is_empty(struct queue_item* fitem) {
	return (fitem->next	== NULL);
}

