#include "proxy_rt.h"

int proxy_load_rt(proxy_manager* manager, char* filename) {

	if( !manager || !filename ) {
		return -1;
	}
	
	FILE* fp = NULL;
	char  line[BUFSIZ];
	char  dest[32];
    char  gw[32];
    char  mask[32];
    char  iface[32];
    struct in_addr dest_addr;
    struct in_addr gw_addr;
    struct in_addr mask_addr;
	struct proxy_rt rt;
	
	memset(line, 0, sizeof(line));
	memset(&rt, 0, sizeof(rt));
	
	if(access(filename, R_OK) != 0) {
        perror("access");
       // return -1;
    }
	fp = fopen(filename, "rw+");
	if( !fp ) {
		return -1;
	}

	while( fgets(line, BUFSIZ, fp) != 0) {
		sscanf(line,"%s %s %s %s",dest,gw,mask,iface);
        if(inet_aton(dest, &dest_addr) == 0) {
            fprintf(stderr,
                    "Error loading routing table, \
                    cannot convert %s to valid IP\n",
                    dest);
            goto err;
        }
        if(inet_aton(gw, &gw_addr) == 0) {
            fprintf(stderr,
                    "Error loading routing table,\
                    cannot convert %s to valid IP\n",
                    gw);
           goto err;
        }
        if(inet_aton(mask, &mask_addr) == 0) {
            fprintf(stderr,
                    "Error loading routing table, \
                    cannot convert %s to valid IP\n",
                    mask);
            goto err;
        }

		rt.dest = dest_addr;
		rt.gw 	= gw_addr;
		rt.mask = mask_addr;
		proxy_add_rt_entry(manager, &rt);
	}
	 
	return 0;

err:
	sfclose(fp);
	return -1;
}


int proxy_add_rt_entry(proxy_manager* manager, struct proxy_rt* rt) {

	if( !manager || !rt ) {
		return -1;
	}

	struct proxy_rt* rt_walker = NULL;
#if 1
	 /* -- empty list special case -- */
    if( !manager->routing_table )
    {
        manager->routing_table = (struct proxy_rt*)malloc(sizeof(struct proxy_rt));
		if( !manager->routing_table ) {
			return -1;
		}
        manager->routing_table->next = NULL;
        manager->routing_table->dest = rt->dest;
        manager->routing_table->gw   = rt->gw;
        manager->routing_table->mask = rt->mask;
       // strncpy(manager->routing_table->interface, if_name, sr_IFACE_NAMELEN);

        return 0;
    }
#endif
	/* -- find the end of the list -- */
    rt_walker = manager->routing_table;
    while(rt_walker->next){
      rt_walker = rt_walker->next;
    }

    rt_walker->next = (struct proxy_rt*)malloc(sizeof(struct proxy_rt));
	if( !rt_walker->next )
		return -1;
	
    rt_walker = rt_walker->next;

    rt_walker->next = 0;
    rt_walker->dest = rt->dest;
    rt_walker->gw   = rt->gw;
    rt_walker->mask = rt->mask;
    //strncpy(rt_walker->interface,if_name,sr_IFACE_NAMELEN);
	
	return 0;
}


void proxy_print_routing_entry(struct proxy_rt* entry)
{
    /* -- REQUIRES --*/
    //assert(entry);
    //assert(entry->interface);

    /* Modified by Syugen for better formatting. */
    printf("%s\t",inet_ntoa(entry->dest));
    printf("%s\t",inet_ntoa(entry->gw));
    printf("%s\t",inet_ntoa(entry->mask));
    //printf("%s\n",entry->interface);

} /* -- sr_print_routing_entry -- */

void proxy_print_routing_table(proxy_manager* sr)
{
    struct proxy_rt* rt_walker = NULL;

    if(!sr->routing_table)
    {
        printf(" *warning* Routing table empty \n");
        return;
    }

    /* Modified by Syugen for better formatting. */
    printf("Destination\tGateway\t\tMask\t\tIface\n");

    rt_walker = sr->routing_table;

    proxy_print_routing_entry(rt_walker);
    while(rt_walker->next) {
        rt_walker = rt_walker->next;
        proxy_print_routing_entry(rt_walker);
    }

} /* -- sr_print_routing_table -- */

