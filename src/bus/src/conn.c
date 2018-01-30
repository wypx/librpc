#include <stdio.h>
#include <string.h>

#include "conn.h"

CONN_LIST* conn_list_init(int size) {
	
	unsigned int len = sizeof(CONN_LIST) + sizeof(CONN) * (size + 1);
	
	CONN_LIST* lst = (CONN_LIST*) malloc(len);
	if ( !lst )
		return NULL;

	memset(lst, 0, len);
	lst->head = &lst->list[0];
	lst->tail = &lst->list[size];
	int i = 0;
	for (i = 0; i < size; i++) {
		lst->list[i].ind = i;
		lst->list[i].next = &lst->list[i + 1];
	}
	lst->list[size].ind = size;
	lst->list[size].next = NULL;

	return lst;
}



