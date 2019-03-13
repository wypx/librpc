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

#include "server.h"

extern struct conn *conn_new(s32 new_fd, s32 event);
extern struct cmd *cmd_new(void);
extern void cmd_free(struct cmd *old_cmd);
extern void conn_free(struct conn *c);
extern struct conn *conn_find_by_id(u32 cid);
extern s32 conn_add_dict(struct conn *c);

s32 thread_init(void);
void thread_deinit(void);


/* Internal functions */
static s32 rx_thread_init(void);
static s32 tx_thread_init(void);
static s32 listen_thread_init(void);
static s32 mgt_thread_init(void);

static void *rx_thread_worker(void *arg);
static void *tx_thread_worker(void *arg);
static void *listen_thread_worker(void *arg);
static void *mgt_thread_worker(void *arg);

static void register_thread_initialized(void);
static void wait_for_thread_registration(void);
static inline void rx_wakeup_write(struct conn *c);


void register_thread_initialized(void) {
    pthread_mutex_lock(&srv->init_lock);
    srv->init_count++;
    pthread_cond_signal(&srv->init_cond);
    pthread_mutex_unlock(&srv->init_lock);
}

void wait_for_thread_registration(void) {

	/* Wait for all the threads to set themselves up before returning. */
	pthread_mutex_lock(&srv->init_lock);
	while (srv->init_count < srv->max_thread * 2) {
        pthread_cond_wait(&srv->init_cond, &srv->init_lock);
    }
	pthread_mutex_unlock(&srv->init_lock);
}


static inline void rx_wakeup_write(struct conn *c) {

	s32 value;

	if (!list_empty(&c->tx->tx_cmd_list)) {
		sem_getvalue (&c->tx->sem, &value);
		if (0 == value) {
			printf("Try to wakeup tx thread(%p) thread_idx(%u)\n", 
					c->tx, c->tx->thread_idx);
			sem_post(&c->tx->sem);
		}
	}
	
	return;
}

void conn_update_rxdesc(struct conn *c, s32 rx_bytes)
{
}

void conn_update_txdesc(struct conn *c, s32 tx_bytes) {
}

void conn_add_tx_iovec(struct conn *c, void *buf, u32 len) {
    c->desc.tx_iov[c->desc.tx_iovcnt].iov_base = buf;
    c->desc.tx_iov[c->desc.tx_iovcnt].iov_len = len;
    c->desc.tx_iovcnt++;
    c->desc.tx_iosize += len;
}

void conn_add_rx_iovec(struct conn *c, void *buf, u32 len) {
    c->desc.rx_iov[c->desc.rx_iovcnt].iov_base = buf;
    c->desc.rx_iov[c->desc.rx_iovcnt].iov_len = len;
    c->desc.rx_iovcnt++;
    c->desc.rx_iosize += len;
}

void init_msghdr(struct msghdr *msg, struct iovec *iov, u32 iovlen) {
    msf_memzero(msg, sizeof(struct msghdr));
    msg->msg_flags = MSG_WAITALL | MSG_NOSIGNAL;
    msg->msg_iov = iov;
    msg->msg_iovlen = iovlen;
}

void rx_handle_result(struct conn *c, s32 rc) {

    if (unlikely(0 == rc)) {
        printf("Recv close by peer fd(%d) ret(%d) errno(%d)\n", 
                c->clifd, rc, errno);
        conn_free(c);
        c->desc.recv_state = io_close;
    } else if (rc < 0) {
        printf("Recvmsg fd(%d) errno(%d)\n", c->clifd, errno);
        if (errno == EINTR || errno == EAGAIN ||
           errno == EWOULDBLOCK) {
            c->desc.recv_state = io_read_half;
            return;
        }
        conn_free(c);
        c->desc.recv_state = io_close;
    } else {
        c->desc.rx_iovoffset += rc;
        if (unlikely((u32)rc < c->desc.rx_iosize)) {
            c->desc.recv_state = io_read_half;
        } else {
            c->desc.recv_state = io_read_done;
        }
    }
}

void tx_handle_result(struct conn *c, s32 rc) {

    if (unlikely(0 == rc)) {
        printf("Send close by peer fd(%d) ret(%d) errno(%d)\n", 
            c->clifd, rc, errno);
        conn_free(c);
        c->desc.send_state = io_close;
    } else if (rc < 0) {
        printf("Sendmsg fd(%d) errno(%d)\n", c->clifd, errno);
        if (errno == EINTR || errno == EAGAIN ||
           errno == EWOULDBLOCK) {
            c->desc.send_state = io_write_half;
            return;
        }
        conn_free(c);
        c->desc.send_state = io_close;
    } else {
        c->desc.tx_iovoffset += rc;
        if (unlikely((u32)rc < c->desc.tx_iosize)) {
            c->desc.send_state = io_write_half;
        } else {
            c->desc.send_state = io_write_done;
        }
    }
}

static s32 rx_thread_login_rsp(struct conn *c) {

    struct basic_head *bhs = NULL;
    struct cmd *new_cmd = NULL;

    bhs = &c->bhs;

    new_cmd = cmd_new();
    if (!new_cmd) {
        /* todo rsp code, expire timer to close fd*/
        return -1;
    }

    MSF_SWAP(&bhs->srcid, &bhs->dstid);
    bhs->opcode = RPC_ACK;
    bhs->errcode = RPC_EXEC_SUCC;

    memcpy(&new_cmd->bhs, &c->bhs, sizeof(struct basic_head));
    new_cmd->cmd_conn = c;

    pthread_spin_lock(&c->tx->tx_cmd_lock);

    list_add_tail(&new_cmd->cmd_to_list, &c->tx->tx_cmd_list);

    pthread_spin_unlock(&c->tx->tx_cmd_lock);

    rx_wakeup_write(c);

    return 0;
}

static s32 rx_thread_proxy_req(struct conn *c) {

    struct basic_head *bhs = NULL;
    struct cmd *new_cmd = NULL;
    struct conn *dst_conn = NULL;
    
    bhs = &c->bhs;

    new_cmd = cmd_new();
    if (!new_cmd) {
        /* todo rsp code, expire timer to close fd*/
        return -1;
    }
    
    dst_conn = conn_find_by_id(bhs->dstid);
    if (unlikely(!dst_conn)) {
        printf("Peer id(%u) offline.\n", bhs->dstid);
        MSF_SWAP(&bhs->srcid, &bhs->dstid);
        bhs->datalen = 0;
        bhs->restlen = 0;
        bhs->opcode = RPC_ACK;
        new_cmd->cmd_conn = c;
        bhs->errcode = RPC_PEER_OFFLINE;
    } else {
        printf("Peer id(%u) online.\n", bhs->dstid);
        new_cmd->cmd_conn = dst_conn;
        bhs->errcode = RPC_EXEC_SUCC;
    }

    memcpy(&new_cmd->bhs, &c->bhs, sizeof(struct basic_head));

    pthread_spin_lock(&c->tx->tx_cmd_lock);

    list_add_tail(&new_cmd->cmd_to_list, &c->tx->tx_cmd_list);

    pthread_spin_unlock(&c->tx->tx_cmd_lock);

    rx_wakeup_write(c);
    return 0;
}

static void rx_thread_handle_bhs(struct conn *c) {

    struct basic_head *bhs = NULL;

    bhs = &c->bhs;

    printf("\n");
    printf("###################################\n");
    printf("bhs:\n");
    printf("bhs version:0x%x\n",bhs->version);
    printf("bhs magic:0x%x\n",  bhs->magic);
    printf("bhs srcid:0x%x\n",  bhs->srcid);
    printf("bhs dstid:0x%x\n",  bhs->dstid);
    printf("bhs opcode:%u\n",   bhs->opcode);
    printf("bhs cmd:%u\n",      bhs->cmd);
    printf("bhs seq:%u\n",      bhs->seq);
    printf("bhs errcode:%u\n",  bhs->errcode);
    printf("bhs datalen:%u\n",  bhs->datalen);
    printf("bhs restlen:%u\n",  bhs->restlen);
    printf("bhs checksum:%u\n", bhs->checksum);
    printf("bhs timeout:%u\n",  bhs->timeout);
    printf("###################################\n");
    printf("\n");

    /* If bhs no data, handle bhs, otherwise goto recv data*/
    if (unlikely(RPC_MSG_SRV_ID == bhs->dstid)) {
        c->desc.recv_stage = stage_recv_next;
        if (RPC_REQ == bhs->opcode) {
            if (RPC_LOGIN == bhs->cmd) {
                c->cid = bhs->srcid;
                if (conn_add_dict(c) < 0) {
                    printf("Add conn to dict fail.\n");
                    conn_free(c);
                }
                c->desc.recv_stage = stage_recv_data;
            } else if (RPC_LOGOUT == bhs->cmd) {
                conn_free(c);
            }
        }
    } else {
        if (RPC_REQ == bhs->opcode || RPC_ACK == bhs->opcode) {
            if (0 == bhs->datalen) {
                printf("Direct send proxy req.\n");
                c->desc.recv_stage = stage_recv_next;
                rx_thread_proxy_req(c);
            } else {
                c->desc.recv_stage = stage_recv_data;
                printf("Need to recv extra payload.\n");
            }
        }
    }
}

static void rx_thread_read_bhs(struct conn *c) {

    s32 rc = -1;
    struct msghdr *msg = &c->desc.rx_msghdr;

    msf_memzero(&c->bhs, sizeof(struct basic_head));
    conn_add_rx_iovec(c, &c->bhs, sizeof(struct basic_head));

    init_msghdr(msg, c->desc.rx_iov, c->desc.rx_iovcnt);
    rc = msf_recvmsg(c->clifd, msg);

    rx_handle_result(c, rc);

    if (io_read_done == c->desc.recv_state) {
        rx_thread_handle_bhs(c);
    } else {
        /* half recv, must return to recv again */
        return;
    }
    return;
}

static void rx_thread_read_data(struct conn *c) {

    s32 rc = -1;
    struct msghdr *msg = NULL;
    struct cmd *new_cmd = NULL;
    struct basic_head *bhs = NULL;
    struct conn *dst_conn = NULL;

    bhs = &c->bhs;
    msg = &c->desc.rx_msghdr;
    new_cmd = cmd_new();
    if (!new_cmd) return;

    printf("Read data len is %u.\n", bhs->datalen);

    c->desc.rx_iovcnt = 0;
    c->desc.rx_iosize = 0;
    conn_add_rx_iovec(c, &new_cmd->cmd_buff, bhs->datalen);

    init_msghdr(msg, c->desc.rx_iov, c->desc.rx_iovcnt);
    rc = msf_recvmsg(c->clifd, msg);

    printf("Recv data len is %u.\n", rc);

    rx_handle_result(c, rc);

    if (io_read_done == c->desc.recv_state) {

        if (unlikely(RPC_MSG_SRV_ID == bhs->dstid)) {
            if (RPC_REQ == bhs->opcode) {
                if (RPC_LOGIN == bhs->cmd) {
                    struct login_pdu *login = (struct login_pdu *)new_cmd->cmd_buff;
                    printf("Login name is (%s) chap(%u).\n", login->name, login->chap);
                    MSF_SWAP(&bhs->srcid, &bhs->dstid);
                    bhs->datalen = bhs->restlen;
                    bhs->restlen = 0;
                    bhs->opcode = RPC_ACK;
                    bhs->errcode = RPC_EXEC_SUCC;
                    msf_memzero(new_cmd->cmd_buff+sizeof(struct basic_head),
                        sizeof(struct login_pdu)-sizeof(struct basic_head));
                }
            }
        }

        dst_conn = conn_find_by_id(bhs->dstid);
        if (unlikely(!dst_conn)) {
            printf("Peer id(%u) offline.\n", bhs->dstid);
            MSF_SWAP(&bhs->srcid, &bhs->dstid);
            bhs->datalen = 0;
            bhs->restlen = 0;
            bhs->opcode = RPC_ACK;
            new_cmd->cmd_conn = c;
            bhs->errcode = RPC_PEER_OFFLINE;
        } else {
            printf("Peer id(%u) online.\n", bhs->dstid);
            new_cmd->cmd_conn = dst_conn;
            bhs->errcode = RPC_EXEC_SUCC;
        }

        memcpy(&new_cmd->bhs, &c->bhs, sizeof(struct basic_head));

        pthread_spin_lock(&c->tx->tx_cmd_lock);

        list_add_tail(&new_cmd->cmd_to_list, &c->tx->tx_cmd_list);

        pthread_spin_unlock(&c->tx->tx_cmd_lock);

        rx_wakeup_write(c);

        c->desc.recv_stage = stage_recv_next;

    }else {
        /* half recv, must return to recv again */
        return;
    }
    return;
}

static void rx_thread_read_loop(struct conn *c) {

    s32 rc = -1;

    do {
        switch (c->desc.recv_stage) {
            case stage_recv_bhs:
                printf("Stage to recv bhs.\n");
                rx_thread_read_bhs(c);
                if (io_read_half == c->desc.recv_state || 
                    io_close == c->desc.recv_state) {
                    return;
                }
                break;
            case stage_recv_data:
                printf("Stage to recv data.\n");
                rx_thread_read_data(c);
                if (io_read_half == c->desc.recv_state || 
                    io_close == c->desc.recv_state) {
                    return;
                }
                break;
            case stage_recv_next:
                printf("Stage to recv next.\n");
                c->desc.recv_state = io_init;
                c->desc.rx_iosize = 0;
                c->desc.rx_iovcnt = 0;   
                c->desc.rx_iovused = 0;   
                c->desc.rx_iovoffset = 0;
                c->desc.recv_stage = stage_recv_bhs;
                return;
            default:
                printf("Stage default exit now.\n");
                return;
        }
    } while(1);
    return;
}


/*
 * A listen socket handler calls an event facility specific io_accept()
 * method.  The method accept()s a new connection and then calls
 * nxt_event_conn_accept() to handle the new connection and to prepare
 * for a next connection to avoid just dropping next accept()ed socket
 * if no more connections allowed.  If there are no available connections
 * an idle connection would be closed.  If there are no idle connections
 * then new connections will not be accept()ed for 1 second.
 */

/* Internal wrapper around 'accept' or 'accept4' to provide Linux-style
 * support for syscall-saving methods where available.
 *
 * In addition to regular accept behavior, you can set one or more of flags
 * EVUTIL_SOCK_NONBLOCK and EVUTIL_SOCK_CLOEXEC in the 'flags' argument, to
 * make the socket nonblocking or close-on-exec with as few syscalls as
 * possible.
 */
static s32 rx_thread_accept(struct conn *c) {

	s16 event = EPOLLIN | EPOLLOUT;
	//s16 event = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLRDHUP | EPOLLHUP;

	s32 stop = false; 
	s32 new_fd = invalid_socket;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(struct sockaddr_storage);

	struct conn *new_conn = NULL;

	do {
		msf_memzero(&addr, sizeof(struct sockaddr_storage));
#if 0
#if defined(HAVE_ACCEPT4) && defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
	new_fd = accept4(c->clifd, addr, addrlen, SOCK_NONBLOCK);
	if (new_fd >= 0 || (errno != EINVAL && errno != ENOSYS)) {
		/* A nonnegative result means that we succeeded, so return.
		 * Failing with EINVAL means that an option wasn't supported,
		 * and failing with ENOSYS means that the syscall wasn't
		 * there: in those cases we want to fall back.  Otherwise, we
		 * got a real error, and we should return. */
		return new_fd;
	}
#endif
#endif

		 /*
	     * The returned socklen is ignored here, because sockaddr_in and
	     * sockaddr_in6 socklens are not changed.  As to unspecified sockaddr_un
	     * it is 3 byte length and already prepared, because old BSDs return zero
	     * socklen and do not update the sockaddr_un at all; Linux returns 2 byte
	     * socklen and updates only the sa_family part; other systems copy 3 bytes
	     * and truncate surplus zero part.  Only bound sockaddr_un will be really
	     * truncated here.
	     */
		new_fd = accept(c->clifd, (struct sockaddr*)&addr, &addrlen);
		if (new_fd < 0) {
			if (errno == EINTR)
				continue;

			if (errno == EPERM) {

			}
			
			if (errno == ENOBUFS || errno == ENOMEM) {
				//没有足够的自由内存.这通常是指套接口内存分配被限制
				//而不是指系统内存不足
			}

			if (errno == EBADF) {
				//描述符无效
			}

			if (errno == EAGAIN || errno == EWOULDBLOCK 
				|| errno == ECONNABORTED) {
				/* these are transient, so don't log anything */
				stop = true;
			} else if (errno == EMFILE || errno == ENFILE) {
				//进程的上限EMFILE
				//系统的上限ENFILE
				fprintf(stderr, "too many open connections\n");	
				//accept_new_conns(s, false);
				stop = true;
			} else {
				perror("accept()");
				stop = true;
			}
			break;
		}

	}while(stop);

	new_conn = conn_new(new_fd, event);
	if (unlikely(!new_conn)) {
		sclose(new_fd);
		return -1;
	}
	
	return 0;
}

void * listen_thread_worker(void *arg) {

    s32 rc = -1;
    s32 idx;
    s32 nfds = invalid_socket;
    struct epoll_event events[max_epoll_event];
    struct conn *c;

    MSF_THREAD_NAME("rpc_listen");

    thread_set_affinity(0);

    while (!srv->stop_flags) {

        nfds = epoll_wait(srv->listen_ep_fd, events, max_epoll_event, -1);
        if (unlikely((0 == nfds) || (nfds < 0 && errno == EINTR))) {
            continue;
        }

        if (unlikely((nfds < 0) && (errno != EINTR))) {
            return NULL;
        }

        for (idx = 0; idx < nfds; idx++) {

        /* 
        * Notice:
        * Epoll event struct's "data" is a union,
        * one of data.fd and data.ptr is valid.
        * refer:
        * https://blog.csdn.net/sun_z_x/article/details/22581411
        */
        c = (struct conn *)events[idx].data.ptr;
        if (unlikely(!c)) 
        continue;

        if (events[idx].events & EPOLLIN) {

        MSF_RPC_LOG(DBG_INFO, "Listen thread fd(%d) happen idx(%d).\n", c->clifd, idx);

        if (c->clifd == srv->unix_socket ||
           c->clifd == srv->net_socket_v4 ||
           c->clifd == srv->net_socket_v4) {
                rx_thread_accept(c);
            }
        } 

        }
    }

    MSF_RPC_LOG(DBG_ERROR, "Listen thread(%ld) exit\n", srv->listen_tid);

    return NULL;
}

void * mgt_thread_worker(void *arg) {

    s32 rc = -1;

    MSF_THREAD_NAME("rpc_mgt");

    thread_set_affinity(0);

    while (!srv->stop_flags) {

        sem_wait_i(&srv->mgt_sem, MSF_WAIT_FOREVER);

        MSF_RPC_LOG(DBG_INFO, "Managent thread do somthing....\n");

    }

    MSF_RPC_LOG(DBG_INFO, "Managent thread(%ld) exit\n", srv->mgt_tid);
    return NULL;
}

static void *rx_thread_worker(void *arg) {

    struct rx_thread *rx = (struct rx_thread *)arg;

    s32 rc = -1;
    s32 idx;
    s32 nfds = invalid_socket;
    struct epoll_event events[max_epoll_event];
    struct conn *c;

    s8 rx_name[32] = { 0 };
    snprintf(rx_name, sizeof(rx_name)-1, "rpc_rx_%d", rx->thread_idx+1);
    MSF_THREAD_NAME(rx_name);

    thread_set_affinity(rx->thread_idx+1);

    register_thread_initialized();

    while (!srv->stop_flags) {

        nfds = epoll_wait(rx->epoll_fd, events, max_epoll_event, -1);
        if (unlikely((0 == nfds) || (nfds < 0 && errno == EINTR))) {
            continue;
        }

        if (unlikely((nfds < 0) && (errno != EINTR))) {
            return NULL;
        }

        for (idx = 0; idx < nfds; idx++) {
            /* 
             * Notice:
             * Epoll event struct's "data" is a union,
             * one of data.fd and data.ptr is valid.
             * refer:
             * https://blog.csdn.net/sun_z_x/article/details/22581411
             */
            c = (struct conn *)events[idx].data.ptr;
            if (unlikely(!c)) 
                continue;

            if (events[idx].events & EPOLLIN) {
                MSF_RPC_LOG(DBG_INFO, "RX thread event happen fd(%d) idx(%d).\n", c->clifd, idx);
                rx_thread_read_loop(c);
            } 

            if (events[idx].events & EPOLLOUT) {
                    rx_wakeup_write(c);
            }

        }
    }

    MSF_RPC_LOG(DBG_INFO, "RX thread(%u) exit\n", rx->thread_idx);

    return NULL;
}

static void * tx_thread_worker(void *arg) {

    struct tx_thread *tx = (struct tx_thread *)arg;

    s32 rc = -1;
    struct cmd *new_cmd = NULL;
    struct conn *c = NULL;

    s8 tx_name[32] = { 0 };
    snprintf(tx_name, sizeof(tx_name)-1, "rpc_tx_%d", tx->thread_idx+1);
    MSF_THREAD_NAME(tx_name);

    thread_set_affinity(tx->thread_idx+1);

    register_thread_initialized();

    while (!srv->stop_flags) {

        sem_wait_i(&tx->sem, MSF_WAIT_FOREVER);

        MSF_RPC_LOG(DBG_INFO, "TX thread try to get one cmd.\n");

        pthread_spin_lock(&tx->tx_cmd_lock);
        if (list_empty(&tx->tx_cmd_list)) {
            pthread_spin_unlock(&tx->tx_cmd_lock);
            continue;
        }

        new_cmd = list_first_entry_or_null(&tx->tx_cmd_list, 
                                             struct cmd, cmd_to_list);
        pthread_spin_unlock(&tx->tx_cmd_lock);

        if (unlikely(!new_cmd)) {
            MSF_RPC_LOG(DBG_ERROR, "Tx thread fail to pop one cmd.\n");
            continue;
        } else {
            list_del_init(&new_cmd->cmd_to_list);
            MSF_RPC_LOG(DBG_INFO, "Tx thread pop one cmd successful.\n");
        }

        c = new_cmd->cmd_conn;
        if (unlikely(!c)) {
            MSF_RPC_LOG(DBG_ERROR, "Cmd(%p) conn is invalid.\n", new_cmd);
            continue;
        }

        c->desc.tx_iovcnt = 0;
        c->desc.tx_iosize = 0;
        conn_add_tx_iovec(c, &new_cmd->bhs, sizeof(struct basic_head));
        if (new_cmd->bhs.datalen > 0) {
            conn_add_tx_iovec(c, &new_cmd->cmd_buff, new_cmd->bhs.datalen);
        }
        init_msghdr(&c->desc.tx_msghdr, c->desc.tx_iov, c->desc.tx_iovcnt);
        rc = msf_sendmsg(c->clifd, &c->desc.tx_msghdr);

        MSF_RPC_LOG(DBG_INFO, "Sendmsg fd(%d) ret(%d).\n", c->clifd, rc);

        tx_handle_result(c, rc);
        if (io_write_done == c->desc.send_state) {
            cmd_free(new_cmd);
        }
    }

    MSF_RPC_LOG(DBG_INFO, "TX thread(%u) exit now.\n", tx->thread_idx);
    return NULL;
}

s32 rx_thread_init(void)
{
    s32 rc;
    s32 idx;
    struct rx_thread *rx;

    srv->rx_threads = calloc(srv->max_thread, sizeof(struct rx_thread));
    if (!srv->rx_threads) {
        MSF_RPC_LOG(DBG_INFO, "Can't allocate rx thread descriptors");
        return -1;
    }

    /* Create threads after we've done all the libevent setup. */
    for (idx = 0; idx < srv->max_thread; idx++) {
        rx = &srv->rx_threads[idx];
        rx->thread_idx = idx;
        rx->epoll_fd = msf_epoll_create();
        if (rx->epoll_fd < 0) return -1;

        rc = pthread_spawn(&rx->tid, rx_thread_worker, rx);
        if (rc < 0) return -1;
    }
    return 0;
}


s32 tx_thread_init(void) {

    s32 rc;
    s32 idx;
    struct tx_thread *tx = NULL;

    srv->tx_threads = calloc(srv->max_thread, sizeof(struct tx_thread));
    if (!srv->tx_threads) {
        MSF_RPC_LOG(DBG_INFO, "Can't allocate tx thread descriptors.");
        return -1;
    }

     /* Create threads after we've done all the libevent setup. */
    for (idx = 0; idx < srv->max_thread; idx++) {
        tx = &srv->tx_threads[idx];
        tx->thread_idx = idx;
        INIT_LIST_HEAD(&tx->tx_cmd_list);
        sem_init(&tx->sem, 0, 0);
        pthread_spin_init(&tx->tx_cmd_lock, 0);

        rc = pthread_spawn(&tx->tid, tx_thread_worker, tx);
        if (rc < 0) return -1;
    }

    return 0;
}

s32 listen_thread_init(void) {

    s32 rc;

    srv->listen_ep_fd = msf_epoll_create();
    if (srv->listen_ep_fd < 0) return -1;

    rc = pthread_spawn(&srv->listen_tid, listen_thread_worker, NULL);
    if (rc < 0) return -1;

    return 0;
}

s32 mgt_thread_init(void) {

    s32 rc = -1;

    sem_init(&srv->mgt_sem, 0, 0);

    rc = pthread_spawn(&srv->mgt_tid, mgt_thread_worker, NULL);
    if (rc < 0) return -1;

    return 0;

}

s32 thread_init(void) {

    if (rx_thread_init() < 0) return -1;

    if (tx_thread_init() < 0) return -1;

    wait_for_thread_registration();

    if (listen_thread_init() < 0) return -1;

    if (mgt_thread_init() < 0) return -1;

    return 0;
}

void thread_deinit(void) {

    s32 idx;
    struct rx_thread *rx = NULL;
    struct tx_thread *tx = NULL;

    if (srv->rx_threads) {
        for (idx = 0; idx < srv->max_thread; idx++) {
            rx = &(srv->rx_threads[idx]);
            sclose(rx->epoll_fd);
            sclose(rx->event_fd);
            sclose(rx->timer_fd);
        }
        sfree(srv->rx_threads);
    }

    MSF_RPC_LOG(DBG_INFO, "RX thread exit sucessful.\n");

    if (srv->tx_threads) {
        for (idx = 0; idx < srv->max_thread; idx++) {
            tx = &(srv->tx_threads[idx]);
            sem_destroy(&tx->sem);
        }
        sfree(srv->tx_threads);
    }

    MSF_RPC_LOG(DBG_INFO, "TX thread exit sucessful.\n");

    sclose(srv->listen_ep_fd);

    MSF_RPC_LOG(DBG_INFO, "Listen thread exit sucessful.\n");

    sem_destroy(&srv->mgt_sem);

    MSF_RPC_LOG(DBG_INFO, "Mgt thread exit sucesful.\n");
}

