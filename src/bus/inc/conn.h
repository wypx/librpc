#include "network.h"

#ifdef 	HAVE_LIBEVENT
#include <event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/thread.h>
#endif


typedef struct _WORKER WORKER;

typedef struct _CONN CONN;
typedef struct _CONN_LIST CONN_LIST;

//#define	CONN_BUF_LEN	2048
#define PACK_HEAD_LEN 	64
#define PAY_BUF_LEN 	1024
#define RES_BUF_LEN 	1024


#define CONN_PER_ALLOC	5

/**
 * Possible states of a connection.
 */
enum conn_states {
    conn_listening,  /**< the socket which listens for connections */
    conn_new_cmd,    /**< Prepare connection for next command */
    conn_waiting,    /**< waiting for a readable socket */
    conn_read,       /**< reading in a command line */
    conn_parse_cmd,  /**< try to parse a command from the input buffer */
    conn_write,      /**< writing out a simple response */
    conn_nread,      /**< reading in a fixed number of bytes */
    conn_swallow,    /**< swallowing unnecessary bytes w/o storing */
    conn_closing,    /**< closing this connection */
    conn_mwrite,     /**< writing out many items sequentially */
    conn_closed,     /**< connection is closed */
    conn_watch,      /**< held by the logger thread as a watcher */
    conn_max_state   /**< Max state value (used for assertion) */
};

/* An item in the connection queue. */
enum conn_modes {
    queue_new_conn,   /* brand new connection. */
    queue_redispatch, /* redispatching from side thread */
};

struct _CONN {
	int 	cfd;
	void* 	ssl_conn;
	enum net_protocol protocol;
	unsigned int cip;
	unsigned int cport;
	struct sockaddr_storage caddr;

	int 	auth;
	void* 	user;
	void*  	pass;

	enum conn_states stat;
	enum conn_modes  mode;
	int 	last_cmd_time;

	struct event event;
    short  ev_flags;
    short  which;   /** which events were just triggered */

	char   *rbuf;   /** buffer to read commands into */
    char   *rcurr;  /** but if we parsed some already, this is where we stopped */
    int    rsize;   /** total allocated size of rbuf */
    int    rbytes;  /** how much data, starting from rcur, do we have unparsed */

    char   *wbuf;
    char   *wcurr;
    int    wsize;
    int    wbytes;

	 /* data for the mwrite state */
    struct iovec*	iov;
    int    iovsize;   /* number of elements allocated in iov[] */
    int    iovused;   /* number of elements used in iov[] */
	
	unsigned int  pid;
	unsigned int  ind;

	struct bufferevent *bev;
	
	char	payload[PAY_BUF_LEN];
	char 	result[RES_BUF_LEN];
	
	unsigned int out_buf_len;


	struct queue_item* qpackets;

	CONN*		 next;
	WORKER*		 owner;
	
}__attribute__((__packed__));

struct _CONN_LIST {
	CONN*	head;
	CONN*	tail;
	CONN 	list[0];
	//pthread_mutex_t lock;
};


CONN_LIST* conn_list_init(int size);


#define PUSH_FREE_CONN(list, item)	\
		list->tail->next = item;	\
		list->tail = item;
#define	POP_FREE_CONN(list, item)	\
		if(list->head != list->tail){	\
			item = list->head;	\
			list->head = list->head->next;	\
		} else {	\
			item = NULL;\
		}

