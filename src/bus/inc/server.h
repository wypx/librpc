#include "conn.h"
#include "queue.h"

#include "gipc_process.h"

typedef struct _WORKER WORKER;
typedef struct _SERVER SERVER;

struct _WORKER {
	pthread_t 	 tid;
	unsigned int pid;
	int 		 sfd;
	CONN*		 c;	
	SERVER*		 s;	

	struct event_base* base;
	struct event notify_event;
	int 		 notify_rfd;
	int 		 notify_wfd;
};


struct _SERVER {
	int 	backlog;
	int		sfd;
	int 	fds[GIPC_MAX_PROCESS];

	struct sockaddr_storage s_addr;
	struct sockaddr_in 		s_sin4;
	struct sockaddr_in6 	s_sin6;
	struct sockaddr_un		s_un;
	struct evconnlistener* 	listener;

	/* 如果多网口,不需要绑定具体本地地址 */
	char listen_addrv4[256]; 	/**< Listening address (IPv4 address or FQDN) */
	char listen_addrv6[256]; 	/**< Listening address (IPv6 address or FQDN) */
	char listen_unixpath[256];  /**< Listening address (UNIX path 	 or FQDN) */

 	unsigned short tcp_port; /**< TCP  listening port */
	unsigned short udp_port; /**< UDP  listening port */
  	unsigned short tls_port; /**< TLS  listening port */
 	unsigned short res;

	unsigned int access_mask;  /* access mask (a la chmod) for unix domain socket */
	
	unsigned int idle_timeout;
	unsigned int read_timeout;
	unsigned int write_timeout;

	unsigned int maxfds;
	unsigned int maxcons;   	/* max workers num*/
	unsigned int workcons;  	/* threads number */
	unsigned int currcons;

	unsigned int maxcore;
	unsigned int daemon;

	char*	 inter;
	unsigned int verbose;
	unsigned int protocol;
	unsigned int ssltype;
	unsigned int tls;		/**< TLS socket support */
	SSL_CTX *	 ssl_ctx;
	

	char nonce_key[256]; /**< Private key used to generate nonce */
 	char ca_file[1024]; /**< CA file */
	char cert_file[1024]; /**< Certificate file */

	struct event_base* 		base;
	struct event			ev_listen;

	

	
	int 			conn_count;

	char 			realm[256]; 		 	/**< Realm */
	unsigned short max_client; 	/**< Max simultanous client */
  	unsigned short max_relay_per_username; /**< Max relay per username */


	WORKER**	workers;	

};





