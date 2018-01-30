/**************************************************************************
*
* Copyright (c) 2017, luotang.me <wypx520@gmail.com>, China.
* All rights reserved.
*
* Distributed under the terms of the GNU General Public License v2.
*
* This software is provided 'as is' with no explicit or implied warranties
* in respect of its properties, including, but not limited to, correctness
* and/or fitness for purpose.
*
**************************************************************************/

#include "network.h"
#include "utils.h"

#include "gipc_version.h"
#include "gipc_command.h"
#include "gipc_process.h"
#include "gipc_message.h"

#include "gipc_dict.h"

#ifdef 	HAVE_LIBEVENT
#include <event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/thread.h>
#endif



//http://itindex.net/detail/53322-rpc-%E6%A1%86%E6%9E%B6-bangerlee
//超时与重试 -- 考虑负载
//Client端屏蔽策略
/*
重试仅针对单次请求，如果一台Server异常，一次请求踩过的坑后续请求还要再踩一遍。
为解决重复踩坑的问题，RPC框架需实现Client对异常Server的自动屏蔽。
屏蔽策略可以这样实现，Client对每个后端Server（IP/port）维护一个评分，
每次请求失败（系统失败或超时）则将分数减一，当分数为0时，将IP/port、
当前时间戳（timestamp）信息写入一块共享内存（block_shm），Client上各进程访问Server前，
先跳过block_shm中记录的IP/port，在一段时间后（如10分钟）再放开该IP/port的访问。

以上屏蔽策略，做到了单机内进程/线程间信息共享，但在模块内甚至全网，
Server异常信息依然没有共享，一台机器趟过的坑其他机器也会趟一遍。
为解决机器间Server异常信息共享问题，我们可以对上面的实现做一些修改：
将屏蔽信息上报到 Zookeeper中心节点，各台机监听相应路径节点，
当有新增节点时拉取IP/port、timestamp信息到本机，存入block_shm，
从而实现机器间屏蔽信息共享。我们把Client端的这种屏蔽策略称为 漏桶屏蔽。

*/


enum account_state {
  	AUTHORIZED, 	/**< Client is authorized to access service */
  	RESTRICTED, 	/**< Client has limited access to service (bandwidth, ...) */
  	REFUSED, 		/**< Client is always refused to access service (i.e. blacklist) */
};

enum gipc_stat {
	gipc_uninit,
	gipc_inited,
};

typedef struct  {
	int		  	stat;
	pthread_t 	tid_event;
	int		  	process_fd;
	char	  	process_name[32];
	int	  		process_id;
	service_cb	process_cb;

	char 		user[32];
  	char		pass[32];
	char 		key[16]; /**< MD5 hash */
  	enum account_state state; /**< Access state */
  	char		server_addr[32];
	int			server_port;

	int			tls;
	char*		ca_file;
	char* 		public_certfile; /**< SSL certificate pathname */
  	char* 		private_keyfile; /**< SSL private key pathname */

	int			dct_len;
	int			dct_alg;
	int 		dct_search;
	dict*		dct_ctx;

	CACHE_POOL	cPool;

	struct event_base*	base;
	struct bufferevent* bev;
	struct event		recv_event;
	struct event 		notify_event;
	int 		 		notify_rfd;
	int 		 		notify_wfd;

//	gipc_thread_pool_t* tp;
}GIPC_PARAM;

static GIPC_PARAM 	gipc_param;
static GIPC_PARAM* 	gipc = &gipc_param;

void* gipc_worker_loop(void* lparam);

static int gipc_verify_msg(gipc_header* head);
static int gipc_handle_req(GIPC_PARAM* lp, gipc_header* head);
static int gipc_handle_ack(GIPC_PARAM* lp, gipc_header* head);


int  pthread_spawn(pthread_t *tid, void (*func)(void *), void *arg)
{
    pthread_attr_t thread_attr;

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
    if (pthread_create(tid, &thread_attr, (void *) func, arg) < 0) {
        perror("pthread_create");
        return -1;
    }

    return 0;
}


static void gipc_base_errcb(int fd, void *arg) {
	sclose(fd);
}

static void gipc_base_readcb(int fd, short events, void *arg)
{
	GIPC_PARAM* lp = (GIPC_PARAM*)arg;
	gipc_header recv_head;
		
	CACHE_POOL_NODE*	cal_node	= NULL;

	int len = -1;
	int head_len =	sizeof(recv_head);

	do {
		memset(&recv_head, 0, sizeof(recv_head)); 

		len = recvn(lp->process_fd, (char*)&recv_head, head_len);
		if(len != head_len) {
		  	if (len == -2 || len == -3) {
				sclose(lp->process_fd);
				lp->stat = gipc_uninit;
				return;
	    	} else {
				printf("read head faild len %d\n", len);
				usleep(200000);
				continue;
	       	} 
		}
		
		if( gipc_verify_msg(&recv_head) < 0 ) {
			sclose(lp->process_fd);
			lp->stat = gipc_uninit;
			return;
		}
		switch(recv_head.opcode) {
			case GIPC_REQUEST:
				printf("recv IPC_REQUEST\n");
				gipc_handle_req(lp, &recv_head);
				break;		
			case GIPC_ACK:
				printf("recv IPC_ACK\n");
				gipc_handle_ack(lp, &recv_head);
				break;
			default:
				break;
		}
	}while(0);
	
}

static void gipc_notify_readcb(int fd, short events, void *arg)
{
	GIPC_PARAM* lp = (GIPC_PARAM*)arg;

	unsigned int  mseq;
	if (read(fd, &mseq, 4) != 4) {
		fprintf(stderr, "notify_pipe read error, errno: %d", errno);
		return;
	}
	
	int len = -1;
	gipc_header*		send_head	= NULL;
	CACHE_POOL_NODE*	req_node	= NULL;


	req_node = (CACHE_POOL_NODE*)gipc_dict_search(lp->dct_ctx, 
							lp->dct_search, mseq);

	 printf("dict count = %zu mseg = %d\n", 
	 		gipc_dict_count(lp->dct_ctx), mseq);
	
	send_head = (gipc_header*)req_node->buf;

	do {
		len = sendn(lp->process_fd, req_node->buf, req_node->use_length, 0);

		printf("send len  %d paylen = %d restlen = %d\n", 
			len,
			send_head->paylen,
			send_head->restlen);
		
		if(len <= 0 ) {
			printf("sendn failed errno %d \n", errno);
			if (EAGAIN == errno || EWOULDBLOCK == errno) {
				usleep(40000);
				continue;
			}
		}
	} while(0);

	if (!send_head->ack)  {
		gipc_dict_remove(lp->dct_ctx, mseq, &lp->cPool);
	}

}

static int gipc_task_handle(void* data, unsigned int len)
{
	if( !data ) {
		return -1;
	}
	int ret = -1;
	gipc_header* header = (gipc_header*)data;
	
	int restlen = header->restlen;

	if( gipc->process_cb ) {
		ret = (gipc->process_cb)((char*)data + sizeof(gipc_header),
							&restlen, header->command);
	} else {
		printf( "ipc_unix_task_handle service_callback NULL\n" );
		return 0;
	}

	if( ret == 0 ) {
		return (restlen + sizeof(gipc_header));
	} else {
		return sizeof(gipc_header);
	}
}

int gipc_init(int id, char* host, char* port, service_cb sercb) {

	int	i 	=  0;
	int res = -1;

	if( id < 0 || id > GIPC_MAX_PROCESS || !host || !port || !sercb ) {
		return -1;
	}

	signal_handler(SIGHUP,  SIG_IGN);
	signal_handler(SIGTERM, SIG_IGN);
	signal_handler(SIGPIPE, SIG_IGN);
	
	gipc->stat = gipc_uninit;
	gipc->tid_event= ~0;
	memset(gipc->process_name, 0, sizeof(gipc->process_name));

	if (0 == strncmp(host, local_host_v4, strlen(local_host_v4))
		|| 0 == strncmp(host, local_host_v6, strlen(local_host_v6))) {
		gipc->process_fd = connect_to_unix_socket(PLIST[id], PLIST[GIPC_DEAMON]);
	} else {
		gipc->process_fd = connect_to_host(host, port);
		
	}
	if(gipc->process_fd < 0) {
		goto err;
	}

	if(socket_timeout(gipc->process_fd, 200000) < 0) {
		goto err;
	}
	usleep(500);
	
	gipc->process_id = -1;
	memset(gipc->process_name, 0, sizeof(gipc->process_name));

	/* Check the process name whther is legal */
	for(i = 0; i < (int)_ARRAY_SIZE(GIPC_PLIST); i++) {
		if(strstr((char*)GIPC_PLIST[i].process_name, PLIST[id])) {
			memcpy(gipc->process_name, GIPC_PLIST[i].process_name, 
					sizeof(gipc->process_name));
			gipc->process_id = i;
			gipc->process_cb = sercb;
			break;
		} 	
	}
	if(GIPC_MAX_PROCESS == i) {
		goto err;
	}
#if 0
	gipc->tp = gipc_thread_pool_config(5);
		
	if (gipc_thread_pool_init_worker(gipc->tp) != 0) {
		printf("ngx_thread_pool_init_worker() failed\n");
		goto err;
	}
#endif

	gipc->dct_len 	 = HSIZE;
	gipc->dct_alg 	 = dct_hashtable2; //default alg
	gipc->dct_search = dct_search;
	gipc->dct_ctx 	 = gipc_dict_init(gipc->dct_alg);

	gipc_cache_pool_init(&gipc->cPool);

	gipc->base = event_base_new();
	if ( !gipc->base ) {
		printf("event_base_new failed!\n");
		goto err;
	} 
#if 0
	gipc->bev = bufferevent_socket_new(gipc->base, -1, BEV_OPT_CLOSE_ON_FREE);
	if (!gipc->bev) {
		fprintf(stderr, "bufferevent_socket_new error: %s\n", 
			strerror(errno));
		goto err;
	}

	bufferevent_socket_connect(gipc->bev, 
			(sockaddr *)&server_addr, 
			sizeof(server_addr));
	
	bufferevent_setcb(bev, server_msg_cb, NULL, event_cb, (void *));
	bufferevent_enable(bev, EV_READ|EV_PERSIST);
#endif

	event_set(&gipc->recv_event, 
			gipc->process_fd, 
			EV_READ | EV_PERSIST , 
			gipc_base_readcb, 
			gipc);

	event_base_set(gipc->base, &gipc->recv_event);

	if (event_add(&gipc->recv_event, 0) != 0) {
		fprintf(stderr, "add listen event error: %s\n", 
			strerror(errno));
		goto err;
	}

	int fds[2];
	if ( pipe(fds) ) {
		fprintf(stderr, "init_workers pipe errno: %d %m\n", errno);
		 goto err;
	}

	gipc->notify_rfd = fds[0];
	gipc->notify_wfd = fds[1];

	event_set(&gipc->notify_event, 
			gipc->notify_rfd, 
			EV_READ | EV_PERSIST , 
			gipc_notify_readcb, 
			gipc);

	event_base_set(gipc->base, &gipc->notify_event);
	
	if (event_add(&gipc->notify_event, 0) != 0) {
		fprintf(stderr, "add listen event error: %s\n", 
			strerror(errno));
		goto err;
	}

	pthread_spawn(&gipc->tid_event, gipc_worker_loop, gipc);

	//pthread_create(&gipc->tid_event, NULL, gipc_worker_loop, gipc);

	gipc->stat = gipc_inited;

	usleep(500);

	if (0 == strncmp(host, local_host_v4, strlen(local_host_v4))
		|| 0 == strncmp(host, local_host_v6, strlen(local_host_v6))) {

	} else {
		gipc_header head;
		memset(&head, 0, sizeof(head));
		head.magic 		= GIPC_MAGIC;
		head.version 	= GIPC_VERSION;
		head.resid 		= gipc->process_id;
		head.desid		= GIPC_DEAMON;
		head.opcode 	= GIPC_REQUEST;
		head.command	= processes_register;

		do {
			int len = sendn(gipc->process_fd, 
				(char*)&head, sizeof(head), 0);

			if(len != sizeof(head) ) {
				printf("sendn register errno %d \n", errno);
				if (EAGAIN == errno || EWOULDBLOCK == errno) {
					usleep(40000);
					continue;
				}
			}
		} while(0);
	}


	return 0;
	
err:
	gipc->stat = gipc_uninit;
	//gipc_thread_pool_exit_worker(gipc->tp);
	gipc_deinit();
	return -1;
}

void* gipc_worker_loop(void* lparam) {
	GIPC_PARAM* lp = (GIPC_PARAM*)lparam;
	event_base_dispatch(lp->base);
	return 0;
}

#if 0
static void gipc_thread_handler(void* data)
{
    GIPC_PARAM* lp = (GIPC_PARAM*)data;
    printf("gipc_thread_handler(), fd %d, tid %d\n",
			lp->process_fd, gipc_thread_tid());

}

int gipc_task_post(void* data, int size, gipc_handler handler) {			
	
	gipc_thread_task_t* task = gipc_thread_task_alloc(size);
	
    gipc_memzero(&task, sizeof(gipc_thread_task_t) + size);

    task->handler = handler;
    task->ctx 	  = data;
    
    if (gipc_thread_task_post(gipc->tp, task) != 0) {
        return -1;
    }
	return 0;
}
#endif


int gipc_verify_msg(gipc_header* head) {
	
	if(GIPC_MAGIC != head->magic) {
		printf("gipc_verify_message not match GIPC_MAGIC !\n");
		return -1;
	}
	
	if(head->resid >= GIPC_MAX_PROCESS || head->desid >= GIPC_MAX_PROCESS)
		return -1;
	
	return 0;
}


int gipc_handle_req(GIPC_PARAM* lp, gipc_header* head) {

	int len = -1;
	int head_len = sizeof(gipc_header);

	CACHE_POOL_NODE*	send_node	= NULL;

	send_node = cache_pool_node_remove(&lp->cPool, 
				MAX(head->restlen, head->paylen) + head_len);
	if(send_node)
	{
		send_node->ccb = gipc_task_handle; 
		send_node->use_length = MAX(head->restlen, head->paylen) + head_len;

		if(head->paylen > 0) {
			len = recvn(gipc->process_fd, 
				(char*)send_node->buf + head_len, head->paylen);
			if(len != (int)head->paylen)
			{
				printf("read head faild real len %d paylen %d , cmd = [%d]\n", 
						len, 
						head->paylen, 
						head->command); 
			}
		}

		memcpy(send_node->buf, head, head_len);

		send_node->ccb(send_node->buf, send_node->use_length);
			
		/* swap process id */
		head->resid = head->resid ^ head->desid;	
		head->desid = head->resid ^ head->desid;
		head->resid = head->resid ^ head->desid;

		head->ack 	 = 0;
		head->opcode = GIPC_ACK;

		head->paylen 	= head->restlen;
		head->restlen 	= 0;
		head->mseq 	   += 1;
		
		send_node->use_length = head_len + head->paylen;
	
		memcpy(send_node->buf, head, head_len);
		

		gipc_dict_insert(lp->dct_ctx, head->mseq, send_node);


		if (write(lp->notify_wfd, (char*)&head->mseq, 4) != 4) {
			printf("Writing requset thread notify pipe error\n");
		}
	}

	return 0;
}

int gipc_handle_ack(GIPC_PARAM* lp, gipc_header* head) {

	int len = -1;
	int head_len = sizeof(gipc_header);


	CACHE_POOL_NODE* ack_node = (CACHE_POOL_NODE*)
				gipc_dict_search(lp->dct_ctx, lp->dct_search, head->mseq - 1);

	if(ack_node) {
		if(head->paylen> 0) {
			len = recvn(lp->process_fd, (char*)ack_node->buf+ head_len, head->paylen);
			if(len != (int)head->paylen)
			{
				printf("read payload faild real len %d restlen %d cmd = [%d]\n", 
						len,
						head->paylen, 
						head->command);	
			}
		}

		sem_post(&ack_node->ack_sem);
	}
	else
	{
		printf("can't find ack node, type:[%d], src:[%s], dst:[%s], seq:[%ld]\n",
				head->command, 
				PLIST[head->desid], 
				PLIST[head->resid], 
				head->mseq);
		/*Just empty the socket read cache.*/
		len = recvn(lp->process_fd, (char*)ack_node->buf + head_len, head->paylen);
		sem_post(&ack_node->ack_sem);
	}
	return len;
}

int gipc_deinit(void) {


	sclose(gipc->notify_rfd);
	sclose(gipc->notify_wfd);
	sclose(gipc->process_fd);

	libevent_global_shutdown();
	event_base_free(gipc->base);

	gipc_cache_pool_deinit(&gipc->cPool);
	
	gipc_dict_clear(gipc->dct_ctx);

	gipc->stat = gipc_uninit;
	
	return 0;
}

int gipc_call_service(gipc_packet* pkt, int timeout) {

	int res = -1;
	int len = 0;

	gipc_header		ghead;
	int	head_len = sizeof(gipc_header);

	CACHE_POOL_NODE* req_node = NULL;

	memset(&ghead, 0, sizeof(ghead));
	ghead.magic 	= GIPC_MAGIC;
	ghead.version 	= GIPC_VERSION;
	ghead.resid 	= gipc->process_id;
	ghead.desid		= pkt->desid;
	ghead.opcode 	= GIPC_REQUEST;
	ghead.command	= pkt->command;
	ghead.paylen	= pkt->paylen;
	ghead.restlen 	= pkt->restlen;
	
	printf("gipc_call_service header = %d payload = %d cmd = %d\n", 
			head_len,
			pkt->paylen,
			pkt->command);

	ghead.ack 			= (GIPC_NO_WAIT == timeout) ? 0: 1;
	ghead.mseq 			= time(NULL);
	ghead.chsum 		= 0;
	ghead.timeout 		= timeout;

	req_node = cache_pool_node_remove(&gipc->cPool, 
							MAX(ghead.paylen, ghead.restlen) + head_len);
	req_node->use_length = pkt->paylen + head_len; 
	memcpy(req_node->buf, &ghead, head_len);
	memcpy((char*)req_node->buf + head_len, pkt->payload, pkt->paylen);

	
	if(GIPC_NO_WAIT == timeout) {
		gipc_dict_insert(gipc->dct_ctx, ghead.mseq, req_node);
	} else {
		sem_init(&req_node->ack_sem, 0, 0);

		//gipc_task_post(ipc, 0, gipc_thread_handler);

		gipc_dict_insert(gipc->dct_ctx, ghead.mseq, req_node);
		if (write(gipc->notify_wfd, (char*)&ghead.mseq, 4) != 4) {
        	printf("Writing call thread notify pipe error\n");
    	}

		res = sem_wait_i(&req_node->ack_sem, timeout);

		/* Check what happened */
		if(-1 == res) 
		{
			#if 0
			if(ETIMEDOUT == errno) {
				printf("sem_timedwait() timedout, cmd [%d]\n", pkt->cmd);
				/*call cancle current packet*/
				sem_destroy(&ack_node->ack_sem);
				((gipc_header*)ack_node->buf)->opcode = IPC_CANCEL;
				((gipc_header*)ack_node->buf)->ack = 0;
				((gipc_header*)ack_node->buf)->paylen  = head_len;
				((gipc_header*)ack_node->buf)->restlen = 0;
				ack_node->use_length = head_len;
				cache_list_add_tail(&ipc->sndQ, (CACHE_NODE*)ack_node);
			}
			else
			#endif
			{
				printf("sem_timedwait() err, cmd [%d]\n", pkt->command);
				sem_destroy(&req_node->ack_sem);
				gipc_dict_remove(gipc->dct_ctx, ghead.mseq, &gipc->cPool);
			}

			return -2;
		} 
		
		res = ((gipc_header*)req_node->buf)->err;
		memcpy(pkt->restload, (char*)req_node->buf + head_len, pkt->restlen);
		sem_destroy(&req_node->ack_sem);
		gipc_dict_remove(gipc->dct_ctx, ghead.mseq, &gipc->cPool);
	}

	return res;
}


