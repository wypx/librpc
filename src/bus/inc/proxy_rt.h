#include "common.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>


struct proxy_rt {
    struct in_addr dest;
    struct in_addr gw;
    struct in_addr mask;
    char   unix_path[128];


	//unsigned short reserved; /*align*/

	int addr_idx;	/* crt. addr. idx. */

	/*statistics*/
	int tx;
	int tx_bytes;
	int errors;
    struct proxy_rt* next;
};


struct proxy_manager {
    int  fd;   			/* socket to server */
    char user[32]; 		/* user name */
    char host[32]; 		/* host name */
    char template[30]; 	/* template name if any */
    unsigned short topo_id;
    struct sockaddr_in 	sr_addr; 		/* address to server */
  //  struct sr_if* 		if_list; 	/* list of interfaces */
    struct proxy_rt* 	routing_table; 	/* routing table */
   // struct sr_arpcache 	cache;   		/* ARP cache */
    pthread_attr_t 		attr;
    FILE* 				logfile;
};


typedef struct proxy_manager proxy_manager;


int proxy_add_rt_entry(proxy_manager* manager, struct proxy_rt* rt);
int proxy_load_rt(proxy_manager* manager, char* filename);
proxy_manager* proxy_manger_init(void);





