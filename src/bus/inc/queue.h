#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <string.h>

#define MAX_PACKET_LEN 1500

struct queue_item {
    char 				packet[MAX_PACKET_LEN];
    unsigned int 		length;
    unsigned int 		srcid;
	unsigned int 		destid; 
    struct queue_item* 	next;
} __attribute__ ((packed)) ;


void queue_push(struct queue_item* fitem, struct queue_item* nitem);
struct queue_item* queue_pop(struct queue_item* item);
struct queue_item* queue_create_item(char* packet, unsigned int length, 
											unsigned int srcid, unsigned int destid);
int queue_is_empty(struct queue_item* fitem);

#endif	


