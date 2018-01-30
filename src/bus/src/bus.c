#include "common.h"
#include "bus.h"

#include "gipc_command.h"

#include "gipc_message.h"

#include <pthread.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/param.h>

SERVER* ss;

static int init_count = 0;
static pthread_mutex_t init_lock;
static pthread_cond_t  init_cond;

/* Lock to cause worker threads to hang up after being woken */
static pthread_mutex_t worker_hang_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t cqi_freelist_lock = PTHREAD_MUTEX_INITIALIZER;



/* Lock for global stats */
static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

static void conn_tcp_acceptcb(struct evconnlistener *listener, 
							evutil_socket_t fd, struct sockaddr *sa, 
							int socklen, void *user_data);
static void conn_tcp_accept_errorcb(struct evconnlistener *, void *);

static void conn_unix_acceptcb(int sfd, short events, void *arg);

static void conn_close (SERVER* s, CONN* c);
static void conn_readcb(struct bufferevent *bev, void *data);
static void conn_writecb(struct bufferevent* bev, void* data);
static void conn_errorcb(struct bufferevent* bev, short events, void* data);
static void conn_driver_machine(CONN* c, SERVER* s);
static int  conn_update_event(CONN *c, const int new_flags);
static const char *state_text(enum conn_states state);


static int server_unix_init(SERVER* s);
static int server_tcp_init(SERVER* s);
static int server_sock_init(SERVER* s);
static int server_workers_init(SERVER* s);
static int server_worker_init(WORKER* w, int i);

void*  server_worker_start(void *arg);


static void register_thread_initialized(void) {
    pthread_mutex_lock(&init_lock);
    init_count++;
    pthread_cond_signal(&init_cond);
    pthread_mutex_unlock(&init_lock);
}

static void wait_for_thread_registration(int nthreads) {

	/* Wait for all the threads to set themselves up before returning. */
	pthread_mutex_lock(&init_lock);

	while (init_count < nthreads) {
        pthread_cond_wait(&init_cond, &init_lock);
    }

	pthread_mutex_unlock(&init_lock);
}

SERVER* server_init(void) {
	
	int  res = -1;

	if( glog_init(NULL, 0) < 0 ) {
		return NULL;
	}
	
	SERVER* s = (SERVER*)malloc(sizeof(SERVER));
	if( !s ) {
		GBUS_LOG(LV_ERROR, RED, "SERVER malloc failed !\n");
		return NULL;
	} 
	memset(s, 0, sizeof(SERVER));

	snprintf(s->listen_unixpath, sizeof(s->listen_unixpath), "%s%s", 
			PREFIX_UNIX_PATH, PLIST[GIPC_DEAMON]);

	s->backlog 	= GIPC_MAX_PROCESS + 5;
	s->maxcons	= GIPC_MAX_PROCESS;
	s->workcons = GIPC_MAX_PROCESS;
	s->currcons = 0;
	s->read_timeout  = 120;
	s->write_timeout = 120;
	s->verbose  = 1;
	s->sfd		=-1;
	s->tcp_port = 9993;
	s->udp_port = 9996;
	s->tls_port = 9999;
	s->access_mask = 1;
	s->protocol = TCPV4;
	s->inter = "192.168.0.110";

	pthread_mutex_init(&init_lock, NULL);
    pthread_cond_init(&init_cond, NULL);
	 
	s->base = event_base_new();
	if ( !s->base ) {
		GBUS_LOG(LV_ERROR, RED, "event_base_new failed.\n");
		return NULL;
	} 

   	if( server_workers_init(s) < 0 ) {
		GBUS_LOG(LV_ERROR, RED, "server_workers_init failed.\n");
		return NULL;
	}
	
	for (unsigned int i = 0; i < s->workcons; i++) {
		s->workers[i] = (WORKER*)malloc(sizeof(WORKER));
		if(!s->workers[i]) {
			GBUS_LOG(LV_ERROR, RED, "server_workers_init WORKER failed.\n");
			return NULL;
		}
		memset(s->workers[i], 0, sizeof(WORKER));
		if( server_worker_init(s->workers[i], i) < 0) {
			GBUS_LOG(LV_ERROR, RED, "server_worker_init [%d] failed \n", i);
			return NULL;
		}	
		s->workers[i]->s = s;
		//fprintf(stderr, "server_worker_init [%d] succ\n", i);
	}
   /* Wait for all the threads to set themselves up before returning. */
   wait_for_thread_registration(s->workcons);

  fprintf(stderr, "server_workers_init all succussful.\n");

	
#define HAVE_SSL 0
#if HAVE_SSL
	if (s->ssltype != SSL_NONE) {
		int r;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
		SSL_library_init();
		ERR_load_crypto_strings();
		SSL_load_error_strings();
		OpenSSL_add_all_algorithms();
#endif
		r = RAND_poll();
		if (r == 0) {
			GBUS_LOG(LV_ERROR, RED, "RAND_poll() failed.\n");
			return NULL;
		}
		s->ssl_ctx = SSL_CTX_new(TLS_method());
	}
#endif
	ss = s;
	return s;
}

int server_start(SERVER* s) {

	//fprintf(stderr, "server_unix_init s->protocol = %d \n", s->protocol);

	int res = -1;

	if( UNIX == s->protocol ) {
		res = server_unix_init( s );
	} else if( TCPV4 == s->protocol || TCPV6 == s->protocol) {
		//res = server_tcp_init( s );
		res = server_sock_init( s );
	} else if ( UDPV4 == s->protocol || UDPV6 == s->protocol){
		res = server_sock_init( s );
	}
	if( res != 0 ) {
		 GBUS_LOG(LV_ERROR, RED, 
		 	"server_start failed, protocol = %d\n", s->protocol);
		return -1;
	} 
	
    /* Give the sockets a moment to open. I know this is dumb, 
     * but the error is only an advisory.
     */
    usleep(1000);

	event_base_dispatch(s->base);

	return 0;
}


void *server_worker_start(void *arg) {
	WORKER* w = (WORKER*)arg;
	register_thread_initialized();
	event_base_dispatch(w->base);
	printf("server_worker_start error, thread_id: %lu\n", w->tid);
	return NULL;
}

/* create a UNIX domain stream socket */
int server_unix_init(SERVER* s) {

	unsigned int len 	= 0;
	struct stat tstat;
    int old_umask;
	
	memset(&s->s_un, 0, sizeof(s->s_un));
	s->s_un.sun_family = AF_UNIX;
	
	s->sfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( s->sfd < 0 ) {
		fprintf(stderr, "socket failed err = %s\n", strerror(errno));
		return -1;
	}

	/* Clean up a previous socket file if we left it around
     */
	if (lstat(s->listen_unixpath, &tstat) == 0) {
        if (S_ISSOCK(tstat.st_mode))
            unlink(s->listen_unixpath);/* in case it already exists */
    }

	strncpy(s->s_un.sun_path, s->listen_unixpath, 
		MIN(sizeof(s->s_un.sun_path)-1, strlen(s->listen_unixpath)) );

	len = offsetof(struct sockaddr_un, sun_path) + strlen(s->s_un.sun_path);

	old_umask = umask( ~(s->access_mask & 0777));

	
	/* bind the name to the descriptor */
	if ( bind(s->sfd, (struct sockaddr *)&s->s_un, len) < 0 ) {
		umask(old_umask);
		fprintf(stderr, "bind %s call failed  errno: %s\n", 
			s->s_un.sun_path,
			strerror(errno));
		return -1;
	}
	umask(old_umask);

	if( socket_nonblocking(s->sfd) < 0 ) {
		return -1;
	}
	if( socket_reuseaddr(s->sfd) < 0 ) {
		return -1;
	}

	if( socket_linger(s->sfd) < 0 ) {
		return -1;
	}
	if( socket_alive(s->sfd) < 0 ) {
		return -1;
	}

	if ( listen(s->sfd, s->backlog) < 0 ) {	
		/* tell kernel we're a server */
		fprintf(stderr, "listen call failed	errno %s\n", strerror(errno));
		return -1;
	}

	event_set(&s->ev_listen, 
			s->sfd, 
			EV_READ | EV_PERSIST , 
			conn_unix_acceptcb, 
			s);

	event_base_set(s->base, &s->ev_listen);

	if (event_add(&s->ev_listen, 0) != 0) {
		fprintf(stderr, "start server add listen event error: %s\n", 
			strerror(errno));
		return -1;
	}

	for (unsigned int i = 0; i < s->workcons; i++)
		s->workers[i]->sfd = s->sfd;

	return 0;

}

/*
 * Sets a socket's send buffer size to the maximum allowed by the system.
 */
static void maximize_sndbuf(const int sfd) {
    socklen_t intsize = sizeof(int);
    int last_good = 0;
    int min, max, avg;
    int old_size;
	
#define MAX_SENDBUF_SIZE (256 * 1024 * 1024)

    /* Start with the default size. */
    if (getsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &old_size, &intsize) != 0) {
        
        perror("getsockopt(SO_SNDBUF)");
        return;
    }

    /* Binary-search for the real maximum. */
    min = old_size;
    max = MAX_SENDBUF_SIZE;

    while (min <= max) {
        avg = ((unsigned int)(min + max)) / 2;
        if (setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, (void *)&avg, intsize) == 0) {
            last_good = avg;
            min = avg + 1;
        } else {
            max = avg - 1;
        }
    }


   fprintf(stderr, "<%d send buffer was %d, now %d\n", sfd, old_size, last_good);
}

int server_sock_init(SERVER* s) {

	unsigned int len = sizeof(struct sockaddr_in);

	int error = -1;
	struct addrinfo *ai;
    struct addrinfo *next;
    struct addrinfo hints = { .ai_flags = AI_PASSIVE,
                              .ai_family = AF_UNSPEC };
    char port_buf[NI_MAXSERV];

	hints.ai_socktype = IS_UDP(s->protocol) ? SOCK_DGRAM : SOCK_STREAM;

	snprintf(port_buf, sizeof(port_buf), "%d",
		IS_UDP(s->protocol) ? s->udp_port: s->tcp_port);
	
    error= getaddrinfo(s->inter, port_buf, &hints, &ai);
    if (error != 0) {
        if (error != EAI_SYSTEM)
          fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(error));
        else
          perror("getaddrinfo()");
        return -1;
    }
	for (next= ai; next; next= next->ai_next) {
		
		if ((s->sfd = socket(next->ai_family, next->ai_socktype, next->ai_protocol)) == -1) {
			  /* getaddrinfo can return "junk" addresses,
             * we make sure at least one works before erroring.
             */
            if (errno == EMFILE) {
                /* ...unless we're out of fds */
                perror("server_socket");
                exit(1);
            }
            continue;
		}
		socket_nonblocking(s->sfd);
#ifdef IPV6_V6ONLY
		int flags = 1;

		if (next->ai_family == AF_INET6) {
			error = setsockopt(s->sfd, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &flags, sizeof(flags));
			if (error != 0) {
				perror("setsockopt");
				close(s->sfd);
				continue;
			}
		}
#endif
		socket_reuseaddr(s->sfd);
		if(IS_UDP(s->protocol))
			 maximize_sndbuf(s->sfd);
		else {
			socket_linger(s->sfd);
			socket_alive(s->sfd);
			socket_tcp_nodelay(s->sfd);
		}


		if (bind(s->sfd, next->ai_addr, next->ai_addrlen) == -1) {
	            if (errno != EADDRINUSE) {
	                perror("bind()");
	                sclose(s->sfd);
	                freeaddrinfo(ai);
	                return -1;
	            }
	            sclose(s->sfd);
	            continue;
	  	} else {
	  		if(!IS_UDP(s->protocol) &&
				listen(s->sfd, s->backlog) < 0) {
				perror("listen()");
				sclose(s->sfd);
				freeaddrinfo(ai);
				return -1;

			}
			if(next->ai_addr->sa_family == AF_INET ||
	                 next->ai_addr->sa_family == AF_INET6) {
				union {
				   struct sockaddr_in in;
				   struct sockaddr_in6 in6;
			   } my_sockaddr;
			   socklen_t len = sizeof(my_sockaddr);
			   if (getsockname(s->sfd, (struct sockaddr*)&my_sockaddr, &len)==0) {
				   if (next->ai_addr->sa_family == AF_INET) {
					   printf("%s INET: %u\n",
							   IS_UDP(s->protocol) ? "UDP" : "TCP",
							   ntohs(my_sockaddr.in.sin_port));
				   } else {
					   printf("%s INET6: %u\n",
							   IS_UDP(s->protocol) ? "UDP" : "TCP",
							   ntohs(my_sockaddr.in6.sin6_port));
				   }
			   }

			}
	  	}
	}

	freeaddrinfo(ai);

	event_set(&s->ev_listen, 
		   s->sfd, 
		   EV_READ | EV_PERSIST , 
		   conn_unix_acceptcb, 
		   s);

	event_base_set(s->base, &s->ev_listen);

	if (event_add(&s->ev_listen, 0) != 0) {
		fprintf(stderr, 
			"start server add listen event error: %s\n", strerror(errno));
	}
	
	for (unsigned int i = 0; i < s->workcons; i++)
		s->workers[i]->sfd = s->sfd;

	return 0;
}

int server_tcp_init(SERVER* s) {

	memset(&s->s_sin4, 0, sizeof(s->s_sin4));
	s->s_sin4.sin_family = AF_INET;
	s->s_sin4.sin_port 	 = htons(s->tcp_port);

	s->listener = evconnlistener_new_bind(
				s->base,
				conn_tcp_acceptcb, 
				(void*)s,
				LEV_OPT_REUSEABLE |
				LEV_OPT_CLOSE_ON_FREE |
				LEV_OPT_CLOSE_ON_EXEC, 
				s->backlog,
				(struct sockaddr*)&s->s_sin4, 
				sizeof(struct sockaddr_in));
	if ( !s->listener ) {
		printf("evconnlistener_new_bind error...\n");
		return -1;
	}
	evconnlistener_set_error_cb(s->listener, conn_tcp_accept_errorcb);

	return 0;
}


static void conn_notify_action(int fd, short event, void *arg) {
	WORKER *w = (WORKER*)arg;
	unsigned int  cmd;
	if (read(fd, &cmd, 4) != 4) {
		fprintf(stderr, "notify_pipe read error, errno: %d", errno);
		return;
	}

	printf("receive_notify_action cmd: %d %s\n", cmd, state_text(cmd));

	conn_driver_machine(w->c, w->s);
}

int server_worker_init(WORKER* w, int i) {
	
	int res = -1;
	int fds[2];

	if( NULL == w ) {
		fprintf(stderr, "server_worker_init w NULL\n");
		return -1;
	}
	if ( pipe(fds) ) {
		 fprintf(stderr, "init_workers pipe errno: %d %m\n", errno);
		return -1;
	}
	
	
	w->pid = i;
	w->notify_rfd = fds[0];
	w->notify_wfd = fds[1];

	w->base = event_base_new();
	if ( !w->base ) {
		fprintf(stderr, "init_workers event base errno: %d\n", errno);
		return -1;
	} 

	event_set(&w->notify_event, 
			   w->notify_rfd, 
			   EV_READ | EV_PERSIST, 
			   conn_notify_action, 
			   w);
	event_base_set(w->base, &w->notify_event);
	if (event_add(&w->notify_event, 0) == -1) {
		 GBUS_LOG(LV_ERROR, GREEN, 
			"init_workers add event errno: %d %m\n", errno);
		return -1;
	}

	w->c = (CONN*)malloc(sizeof(CONN));
	if( !w->c ) {
		 GBUS_LOG(LV_ERROR, RED, "server_worker_init c NULL\n");
		return -1;
	}
	memset(w->c, 0, sizeof(CONN));
	//w->c->qpackets = queue_create_item(NULL, -1, -1, -1);
	w->c->cfd = -1;
	w->c->protocol 	= UNIX;
	w->c->stat 		= conn_listening;
	w->c->auth		= 0;

	w->c->bev = bufferevent_socket_new(
						w->base, 
						-1, 
						BEV_OPT_CLOSE_ON_FREE);
	if( !w->c->bev ) {
		 GBUS_LOG(LV_ERROR, RED,
			"bufferevent_socket_new failed	errno %d\n", errno);
		return -1;
	} 
	
	bufferevent_setcb(w->c->bev,
					conn_readcb,
					NULL,
					conn_errorcb,
					w);
	
	bufferevent_settimeout(w->c->bev, 120, 120);
	w->c->owner = w;
	
	res = pthread_create(&w->tid, NULL, server_worker_start, w);
	if (res != 0) {
		fprintf(stderr, "start_workers create thread errno: %d %m\n", errno);
		return -1;
	}
	
	return 0;
}



static inline int conn_verify_msg(gipc_header*  head) {

	if(GIPC_MAGIC != head->magic) {
		printf("conn_verify_msg not match GIPC_MAGIC !\n");
		return -1;
	}
	
	if(head->resid >= GIPC_MAX_PROCESS || head->desid >= GIPC_MAX_PROCESS)
		return -1;
	
	
	return 0;
}

static inline int conn_debug_msg(gipc_header*  head) {
	
	printf("\n###########conn_debug_msg#################\n\n");
	
	printf("resid : %s\n",		PLIST[head->resid]);
	printf("desid : %s\n",		PLIST[head->desid]);
	printf("magic : 0x%x\n", 	head->magic);
	printf("version : 0x%x\n", 	head->version);
	
	printf("command : %d\n", 	head->command);
	printf("paylen : %d\n", 	head->paylen);
	printf("restlen : %d\n", 	head->restlen);
	printf("ack : %d\n", 		head->ack);
	printf("err : %d\n", 		head->err);
	printf("mseq : %ld\n",		head->mseq);
	printf("timeout : %d\n",	head->timeout);

	printf("\n###########conn_debug_msg#################\n");

	return 0;
}

static inline int conn_hand_payload(gipc_header* head, WORKER* w) {

	int head_len = sizeof(gipc_header);
	int rlen = -1;
	int slen = head->paylen;

	memcpy(w->c->payload, head, head_len);
	
	if ( head->paylen > 0) {
		do {
			 rlen = bufferevent_read(w->c->bev, w->c->payload + head_len, head->paylen);
			 if (rlen <= 0) {
				 return -1;
			 } else if(rlen == (int)head->paylen) {
				return rlen;
			 }
			 slen 	-= rlen;
		} while (slen > 0);
	}
	return 0;
}

static inline int conn_route_msg(gipc_header* head, WORKER* w)
{
	int len = -1;
	int head_len = sizeof(gipc_header);
	
	if(GIPC_DEAMON == head->desid) {
		
		switch(head->command) {
			/*inform daemon the address of end point process*/
			case screen_output_open: 
				printf("conn_route_msg: IPC_DEAMON CMD_OUTPUT_OPEN.\n");
				break;
			case screen_output_close: 
				printf("conn_route_msg: IPC_DEAMON CMD_OUTPUT_CLOSE.\n");
				break;
			default:
				printf("conn_route_msg: IPC_DEAMON NO SUCH COMMAND.\n");
				break;
		}
	//数据转发
	} else {
	
		if(w->s->fds[head->desid] <= 0) {
			printf("send failed , peer offline\n");

			if(head->opcode == GIPC_REQUEST)
				head->opcode = GIPC_ACK;
			head->restlen = 0;
			head->err   = 1;
			head->mseq += 1;
			len = sendn(bufferevent_getfd(w->c->bev), 
										(char*)head, 
										head_len, 
										head->timeout);
			if (len  <= 0) {
				printf("send err failed errno %d \n", errno);
			}
			return -1;
		}
		
		len = sendn(w->s->fds[head->desid], 
					w->c->payload, head->paylen + head_len, head->timeout);
		if (len  <= 0) {
			printf("send header failed errno %d \n", errno);
			return -1;
		}

	}
	return 0;
}


int server_destroy(SERVER* s) {
	
	sclose(s->sfd);

	libevent_global_shutdown();

	unlink(s->listen_unixpath);

	CONN* 	c = NULL;

	for(unsigned int i = 0; i < s->workcons; i++) {
		c = s->workers[i]->c;
		if( c->bev ) {
			/* None of the other events can happen here, 
			since we haven't enabled timeouts */
			bufferevent_free(c->bev);
		}
		sclose(c->cfd);
		sfree(c);
		sfree(s->workers[i]);
	}
	sfree(s->workers);
	
	if( !s->listener ) {
		evconnlistener_free(s->listener);
	}
	event_base_free(s->base);
	sfree(s);

	return 0;
}


/**
 * Convert a state name to a human readable form.
 */
static const char *state_text(enum conn_states state) {
    const char* const statenames[] = { "conn_listening",
                                       "conn_new_cmd",
                                       "conn_waiting",
                                       "conn_read",
                                       "conn_parse_cmd",
                                       "conn_write",
                                       "conn_nread",
                                       "conn_swallow",
                                       "conn_closing",
                                       "conn_mwrite",
                                       "conn_closed",
                                       "conn_watch" };
    return statenames[state];
}

/*
 * Sets a connection's current state in the state machine. Any special
 * processing that needs to happen on certain state transitions can
 * happen here.
 */
static void conn_set_state(CONN *c, enum conn_states state) {
    if( !c  || state < conn_listening ||  state >= conn_max_state) {
		return ;
	}

    if (state != c->stat) {
            fprintf(stderr, "[%s][%d]:state going from %s to %s\n",
                    PLIST[c->pid], 
                    c->cfd,
                    state_text(c->stat),
                    state_text(state));

        if (state == conn_write || state == conn_mwrite) {
        
        }
        c->stat = state;
    }
}

/*
 * Initializes the connections array. We don't actually allocate connection
 * structures until they're needed, so as to avoid wasting memory when the
 * maximum connection count is much higher than the actual number of
 * connections.
 *
 * This does end up wasting a few pointers' worth of memory for FDs that are
 * used for things other than connections, but that's worth it in exchange for
 * being able to directly index the conns array by FD.
 */
static int server_workers_init(SERVER* s) {
	/* We're unlikely to see an FD much higher than maxconns. */
	int next_fd = dup(1);
	int headroom = 10;	/* account for extra unexpected open FDs */
	struct rlimit rl;

	s->maxfds = s->maxcons + headroom + next_fd;

	/* But if possible, get the actual highest 
		FD we can possibly ever see. */
	if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
		s->maxfds = rl.rlim_max;
	} else {
		fprintf(stderr, 
			"Failed to query maximum file descriptor; "
						"falling back to maxconns\n");
	}
	fprintf(stderr, "server_workers_init maxfds [%d] \n", s->maxfds);

	s->maxfds = GIPC_MAX_PROCESS + 5;

	sclose(next_fd);

	s->workers = malloc((s->maxfds) * sizeof(WORKER*));
	memset(s->workers, 0, (s->maxfds) * sizeof(WORKER*));
	if ( !s->workers ) {
		fprintf(stderr, 
			"Failed to allocate connection workers\n");
		/* This is unrecoverable so bail out early. */
		return -1;
	}
	
	return 0;
}

CONN* conn_new(SERVER* s, enum net_protocol protocol, int cfd) {
	CONN* c;

	if(s->sfd < 0 || s->sfd < (int)s->maxfds)
		return NULL;

	c = s->workers[cfd]->c;
	if( NULL == c ) {
		c = (CONN*)malloc(sizeof(CONN));
		if( NULL == c ) {
			fprintf(stderr, "Failed to allocate connection object\n");
			return NULL;
		}
		memset(c, 0, sizeof(CONN));
	 	c->iovused = 0;
		c->iov = (struct iovec *)malloc(sizeof(struct iovec) * c->iovsize);

		c->cfd= cfd;
    	s->workers[cfd]->c= c;
	}
	c->protocol = protocol;
	c->stat 	= conn_listening;
	c->auth		= 0;

	return c;
}

static void conn_close (SERVER* s, CONN* c) {

	if( !c ) {
		return ;
	}
    /* delete the event, the socket and the conn */
    event_del(&c->event);
	
	conn_set_state(c, conn_closed);

		
	memset(c->payload,  0, sizeof(c->payload));
	memset(c->result,   0, sizeof(c->result));

	//bufferevent_disable(c->bev, EV_READ | EV_WRITE);

	sclose(c->cfd);

	s->fds[c->pid] = -1;
	
	//PUSH_FREE_CONN(c->owner->conns, c);

	pthread_mutex_lock(&stats_lock);
	s->currcons--;
	pthread_mutex_unlock(&stats_lock);
}

static void conn_free (SERVER* s, CONN* c) {

	if( !c || c->cfd < 0) {
		fprintf(stderr, "conn_close conn null\n");
		return ;
	}
	fprintf(stderr, "<%d connection closed.\n", c->cfd);
	
	//conns[c->cfd] = NULL;
	
	sclose(c->cfd);
	
    /* delete the event, the socket and the conn */
    event_del(&c->event);
	
	bufferevent_disable(c->bev, EV_READ | EV_WRITE);

	sfree(c);


}

static int conn_update_event(CONN *c, const int new_flags) {
   	if ( NULL == c ) 
   		return -1;

   	if (c->ev_flags == new_flags) {
        return 0;
	}
   
	if( bufferevent_setfd(c->bev, c->cfd) < 0 )
		return -1;
	//bufferevent_setwatermark(w->conns->list[ind].bev, EV_READ, 0, MAX_LINE);
	//bufferevent_setwatermark(partner, EV_WRITE, MAX_OUTPUT/2, MAX_OUTPUT);
	if( bufferevent_enable(c->bev, new_flags) < 0 )
		return -1;
	//EV_PERSIST | EV_CLOSED | EV_FINALIZE | EV_TIMEOUT

	c->ev_flags = new_flags;
    return 0;
}

static void conn_driver_machine(CONN* c, SERVER* s) {

	fprintf(stderr, "conn_driver_machine cmd = %s\n",
		state_text(c->stat));
	switch(c->stat) {
		   case conn_listening:
		   		c->mode = queue_new_conn;
		   		break;
			
			case conn_waiting:
				if( conn_update_event(c, EV_READ) < 0 ) {
					conn_set_state(c, conn_closing);
				}
				conn_set_state(c, conn_read);
				break;
			case conn_read:
				//no data
				//conn_set_state(c, conn_waiting);
				//paser
				conn_set_state(c, conn_parse_cmd);
				//read error
				//conn_set_state(c, conn_closing);
				
				break;
		  	case conn_parse_cmd :
				//read 
				conn_set_state(c, conn_waiting);
				break;
			case conn_new_cmd:
				if( conn_update_event(c, EV_WRITE) < 0 ) {
					conn_set_state(c, conn_closing);
				}
				break;
			case conn_nread:
				break;
			case conn_swallow:
				break;
			case conn_write:
				break;
		 	case conn_mwrite:
				//洗完
				 conn_set_state(c, conn_new_cmd);
			
				break;
			case conn_closing:
				conn_close(s, c);
				break;
			case conn_closed:
				/* This only happens if dormando is an idiot. */
				abort();
				break;
			case conn_watch:
            	/* We handed off our connection to the logger thread. */
          
            	break;
			case conn_max_state:
				break;
			default:
				break;
	}
#if 0
	//notify worker to enable cfd's read and write
	if (write(c->owner->notify_wfd, (char*)&c->stat, 4) != 4) {
        printf("Writing to thread notify pipe error\n");
    } else 
     	printf("Writing to thread notify pipe succ \n");
#endif
}

static void conn_unix_acceptcb(int sfd, short events, void *arg) {

	SERVER* s = (SERVER*) arg;

	CONN* c = NULL;

	int stop = 0;

	socklen_t len = sizeof(struct sockaddr_storage);
	
	struct sockaddr_storage addr;
	 
	int cfd = -1;
	int res = -1;

	int id 	= 0;

#ifdef HAVE_ACCEPT4
    static int  use_accept4 = 1;
#else
    static int  use_accept4 = 0;
#endif

	do {
		memset(&addr, 0, sizeof(struct sockaddr_storage));

#ifdef HAVE_ACCEPT4
        if (use_accept4) {
            cfd = accept4(s->sfd, (struct sockaddr *)&addr, &len,
							SOCK_NONBLOCK);
        } else {
            cfd = accept(s->sfd, (struct sockaddr *)&addr, &len);
        }
#else
		cfd = accept(s->sfd, (struct sockaddr*)&addr, &len);
#endif
		if (cfd < 0) {
			if( use_accept4 && errno == ENOSYS) {
				use_accept4 = 0;
				continue;
			}
			if (errno == EINTR)
				continue;

			if (errno == EAGAIN 
				|| errno == EWOULDBLOCK 
				|| errno == ECONNABORTED) {
                /* these are transient, so don't log anything */
                stop = 1;
            } else if (errno == EMFILE) {
                fprintf(stderr, "too many open connections\n");
                stop = 1;
            } else {
                perror("accept()");
                stop = 1;
            }
			
			fprintf(stderr, "cannot accpet, errno: %d %m\n", errno);
			break;
		}

		//如果超过FD_SETSIZE 1024可能会接受失败
		if(cfd > FD_SETSIZE) {
			fprintf(stderr, "conn_unix_acceptcb FD_SETSIZE\n");
			sclose(cfd);
			break;
		}

		if(addr.ss_family == AF_UNIX) { 
					
			struct sockaddr_un* cun = (struct sockaddr_un*)&addr;
	
			printf("conn_unix_acceptcb, peer[%s] cfd[%d]\n",
				cun->sun_path, cfd);
			
			for(id = 0; id < GIPC_MAX_PROCESS; id++) {	
				if(strstr(cun->sun_path, PLIST[id])) {	
					s->fds[id] = cfd;
					break;
				}
			}
			if(GIPC_MAX_PROCESS == id) {
				printf("conn_unix_acceptcb, IPC_MAX_PROCESS.\n");
				sclose(cfd);
				return;
			}

			c = s->workers[id]->c;
			if( c->cfd > 0 ) {
				conn_close(s, c);
			}
			c->pid = id;
			c->cfd = cfd;
			memcpy(&c->caddr, &addr, len);
	
		}
		else if( addr.ss_family == AF_INET ||
				addr.ss_family == AF_INET6 ) {
	
			struct sockaddr_in* sin = (struct sockaddr_in*)&addr;  
				
			printf("conn_sock_acceptcb, ip:[%s] port[%d]\n", 
				inet_ntoa(sin->sin_addr), ntohs(sin->sin_port));
			
			gipc_header head;
			memset(&head, 0, sizeof(head));
			int len = -1;

			do {
				len = recvn(cfd, (char*)&head, sizeof(head));
				if(len != sizeof(head)) {
					if (len == -2 || len == -3) {
						sclose(cfd);
						return;
			    	} else {
						printf("read reg head faild len %d\n", len);
						usleep(200000);
						continue;
			       	} 
				}
			} while(0);

			if(conn_verify_msg(&head) < 0) {
				sclose(cfd);
				return;
			}

			if(processes_register == head.command) {
				s->fds[head.resid] = cfd;

				c = s->workers[head.resid]->c;
				if( c->cfd > 0 ) {
					conn_close(s, c);
				}
				
				c->pid = head.resid;
				c->cfd = cfd;
				memcpy(&c->caddr, &addr, len);
			}

			//getpeername(sock, (struct sockaddr*)&sa, &salen)
			//citem->cip = *(unsigned int *) &s_in.sin_addr;
			//citem->cport = (uint16) s_in.sin_port;
		}

		//阻塞模式设置放在最后面,为了不影响接收判断附加的进程ID
		if (!use_accept4) {
			res = socket_nonblocking(cfd);
			if (res != 0) {
				printf("socket_nonblocking failed, client_fd: %d\n", cfd);
				sclose(cfd);
				break;
			}
		}

		conn_set_state(c, conn_waiting);

		bufferevent_setfd(c->bev, c->cfd);
		bufferevent_enable(c->bev, EV_READ | EV_PERSIST);
		bufferevent_disable(c->bev, EV_WRITE);
	
		conn_driver_machine(c, s);
		
		return;
	}while(!stop);
	
	printf("conn_unix_acceptcb workers are too busy!\n");
	sclose(cfd);
}

static void conn_tcp_accept_errorcb(struct evconnlistener *listener, void *user_data) {

	struct event_base* base = evconnlistener_get_base(listener);
	int err = EVUTIL_SOCKET_ERROR();

	printf("err is :%d - %s\n", err, evutil_socket_error_to_string(err));
	event_base_loopbreak(base);
}

static void conn_tcp_acceptcb(struct evconnlistener *listener, 
						evutil_socket_t fd, struct sockaddr *sa, int socklen, 
						void *user_data) 
{					
	SERVER* s = (SERVER*)user_data;

	//struct event_base* base = evconnlistener_get_base(listener);
	//evutil_socket_t sfd = evconnlistener_get_fd(listener);
	struct bufferevent *bev;

	bev = bufferevent_socket_new(s->base, fd, BEV_OPT_CLOSE_ON_FREE);
	if ( !bev ) {
		printf("Error constructing bufferevent!");
		event_base_loopbreak(s->base);
		return;
	}

	bufferevent_setcb(bev, 
			conn_readcb, 
			conn_writecb, 
			conn_errorcb, 
			s);
	
	bufferevent_enable(bev, EV_READ);
	bufferevent_disable(bev, EV_WRITE);

}

int evutils_write(struct bufferevent *bev, char* buf, int len) {

	evbuffer_unfreeze(bufferevent_get_output(bev), 1);

	int wlen = 0;
	int slen = 0;
	while (slen < len) {
		
		wlen = bufferevent_write(bev, 
								buf + slen, 
								len - slen);

		printf("bufferevent_write wsize:%d slen:%d\n", wlen, slen);
		
		if( wlen == len ) {
			return wlen;
		} else if (wlen <= 0 ) {
			return -1;
		}

		slen += wlen;
	}
	return slen;
}

int evutils_read(struct bufferevent *bev, char* buf, int len) {

	int rlen = 0;
	int slen = 0;
	char* _buf = buf;

	do {
		 rlen = bufferevent_read(bev, _buf, len);
		 printf("bufferevent_read rsize:%d slen:%d\n", rlen, slen);
		 if (rlen <= 0) {
			 return -1;
		 } else if( rlen == len ) {
			return rlen;
		 }
		 len 	-= rlen;
		 _buf 	+= rlen;

	} while (len > 0);
	
	 return (int) (_buf - buf);
}

static void conn_readcb(struct bufferevent *bev, void *data) {

	WORKER* w = (WORKER*)data;

	SERVER* s = w->s;
	
	int head_len = sizeof(gipc_header);
	int bufflen = evbuffer_get_length(bufferevent_get_input(bev));

	printf("evbuffer_get_length input: %d\n",  bufflen); 
	if( bufflen < head_len) {
		printf("evbuffer_get_length input: %d\n",  bufflen);   
	}
	memset(w->c->payload, 0, sizeof(w->c->payload));
	memset(w->c->result,  0, sizeof(w->c->result));

	gipc_header head;
	
	memset(&head, 0, sizeof(head));

	if(head_len != (int)bufferevent_read(bev, (char*)&head, head_len)) {
		printf("bufferevent_read client header failed.\n");
		goto err;
	}
	//if( evutils_read(bev, (char*)&head, head_len) < 0) {
	//	printf("evutils_read client header failed.\n");
	//	goto err;
	//}

	conn_debug_msg(&head);

	if( conn_verify_msg(&head) < 0 ) {
		printf("conn_verify_msg failed.\n");
		goto err;
	}

	if( conn_hand_payload(&head, w) < 0 ) {
		printf("conn_hand_paload failed.\n");
		goto err;
	}

	if( conn_route_msg(&head, w) < 0 ) {
		printf("conn_route_msg failed.\n");
		return ;
	}
	
	return;
err:
	conn_close(w->s, w->c);
}
static void conn_writecb(struct bufferevent* bev, void* data){

	printf("conn_writecb.......\n");
	return ;

	CONN* c = (CONN*)data;
	//conn->rbuf = bufferevent_get_input(bev);
	//conn->wbuf = bufferevent_get_output(bev);

	gipc_header h;
	memset(&h, 0, sizeof(h));

	h.command = 100;
	bufferevent_write(bev, (char*)&h, sizeof(h));

	///
	(int)bufferevent_getfd(bev);
	struct evbuffer *output = bufferevent_get_output(bev);
	if (evbuffer_get_length(output) == 0) {
		printf("flushed answer\n");
		//bufferevent_disable(bev,EV_READ|EV_WRITE);
		//bufferevent_free(bev);
	}

	conn_update_event(c, EV_READ);
	bufferevent_disable(c->bev, EV_WRITE);
////

//bufferevent_set_timeouts(struct bufferevent * bufev,const struct timeval * timeout_read,const struct timeval * timeout_write)

//bufferevent_write_buffer(struct bufferevent * bufev,struct evbuffer * buf)
//bufferevent_write(struct bufferevent * bufev,const void * data,size_t size)

}

static void conn_errorcb(struct bufferevent* bev, short events, void* data) {

	WORKER* w = (WORKER*)data;

	if (events & BEV_EVENT_EOF) {
		printf("conn_errorcb BEV_EVENT_EOF: %s\n", strerror(errno));
	} else if (events & BEV_EVENT_ERROR) {
		printf("conn_errorcb BEV_EVENT_ERROR: %s\n", strerror(errno));/*XXX win32*/
	}else if (events & BEV_EVENT_TIMEOUT) {
		printf("conn_errorcb timeout : %s\n", strerror(errno));/*XXX win32*/
		return;
	}
	
	conn_close(w->s, w->c);	
}




#define CONNS_PER_SLICE 100
#define TIMEOUT_MSG_SIZE (1 + sizeof(int))

volatile int current_time;

static void *conn_timeout_thread(void *arg) {

	
	char buf[TIMEOUT_MSG_SIZE];
	int oldest_last_cmd;

	CONN* c;
	SERVER* s = (SERVER*)arg;

	useconds_t timeslice = 1000000 / (s->maxfds / CONNS_PER_SLICE);


    while(1) {
		fprintf(stderr, 
			"idle timeout thread at top of connection list\n");

		oldest_last_cmd = current_time;
	    for (unsigned int i = 0; i < s->maxfds; i++) {
	        if ((i % CONNS_PER_SLICE) == 0) {
	               fprintf(stderr,
				   	"idle timeout thread sleeping for %ulus\n",
	                    (unsigned int)timeslice);
	            usleep(timeslice);
	        }
			if(!s->workers[i]->c)
	            continue;

	        c = s->workers[i]->c;

			if(!IS_TCP(c->protocol))
	            continue;

	        if(c->stat!= conn_new_cmd && c->stat!= conn_read)
	            continue;

			if((current_time - c->last_cmd_time) > (int)s->idle_timeout) {
	                buf[0] = 't';
	                memcpy(&buf[1], &i, sizeof(int));
	                if (write(c->owner->notify_wfd, buf, TIMEOUT_MSG_SIZE)
	                    != TIMEOUT_MSG_SIZE)
	                    perror("Failed to write timeout to notify pipe");
	            } else {
	                if (c->last_cmd_time < oldest_last_cmd)
	                    oldest_last_cmd = c->last_cmd_time;
	            }
		}


		 /* This is the soonest we could have another connection time out */
	       int  sleep_time = s->idle_timeout - (current_time - oldest_last_cmd) + 1;
	        if(sleep_time <= 0)
	           sleep_time = 1;

	        fprintf(stderr,
	                    "idle timeout thread finished pass, sleeping for %ds\n",
	                    sleep_time);
	        usleep((useconds_t) sleep_time * 1000000);
	}
	return NULL;
}


int server_conn_timeout_check(SERVER* s) {

	/* Connection timeout thread bits */
	pthread_t conn_timeout_tid = ~0 ;

	if(!s || s->idle_timeout == 0)
		return -1;
	
	if( pthread_create(&conn_timeout_tid, NULL,
        conn_timeout_thread, s) != 0) {
        fprintf(stderr, 
			"Can't create idle connection timeout thread: %s\n",
            strerror(errno));
        return -1;
    }
	
    return 0;
}

