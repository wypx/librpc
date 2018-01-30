#include "proxy_rt.h"

proxy_manager* proxy_manger_init(void) {

	proxy_manager* manager = (proxy_manager*) malloc(sizeof(proxy_manager));
	if( ! manager ) {
		return NULL;
	}

	manager->fd = -1;
	manager->user[0] = 0;
    manager->host[0] = 0;
    manager->topo_id = 0;
   // manager->if_list = 0;
    manager->routing_table = 0;
    manager->logfile = 0;


	proxy_load_rt(manager, "/home/route_rt.conf");

	return manager;
}

