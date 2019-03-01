/**************************************************************************
*
* Copyright (c) 2018, luotang.me <wypx520@gmail.com>, China.
* All rights reserved.
*
* Distributed under the terms of the GNU General Public License v2.
*
* This software is provided 'as is' with no explicit or implied warranties
* in respect of its properties, including, but not limited to, correctness
* and/or fitness for purpose.
*
**************************************************************************/
#include <msf_utils.h>
#include <msf_list.h>


#define MAX_PUBLISH_EVENETS 	32


enum event_type
{
	LINK_UP, 
	LINK_DOWN, 
	COMPONENT_ONLINE, 
	COMPONENT_OFFLINE, 
};


typedef struct __attribute__((__packed__)) {
	struct list_head * head;
	s8				component_name[32]; 			/*	One process that interested in one event */
} subscriber;


typedef struct __attribute__((__packed__)) {
	subscriber *	head;							/* Storage all component name subscribing this event*/
	enum event_type event_idx;
	s8				event_name[32];
} publish_event;




s32 notify_init(void);


static publish_event g_publist_event[MAX_PUBLISH_EVENETS];

s32 notify_init(void) {

	g_publist_event[0].event_idx = COMPONENT_ONLINE;
	memcpy(g_publist_event[0].event_name, "component_online", 
							strnlen("component_online", 32));

	g_publist_event[1].event_idx = COMPONENT_OFFLINE;
	memcpy(g_publist_event[1].event_name, "component_offline", 
							strnlen("component_offline", 32));
	
	return 0;
}



