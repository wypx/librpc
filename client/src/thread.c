/**************************************************************************
*
* Copyright (c) 2017-2018, luotang.me <wypx520@gmail.com>, China.
* All rights reserved.
*
* Distributed under the terms of the GNU General Public License v2.
*
* This software is provided 'as is' with no explicit or implied warranties
* in respect of its properties, including, but not limited to, correctness
* and/or fitness for purpose.
*
**************************************************************************/

#include <client.h>

extern struct client *rpc;

extern struct cmd *cmd_new(s32 data_len);
extern void cmd_free(struct cmd *old_cmd);
extern void cmd_push_tx(struct cmd *tx_cmd);
extern void cmd_push_tx_head(struct cmd *tx_cmd);
extern void cmd_push_ack(struct cmd *ack_cmd);;
extern struct cmd* cmd_pop_tx(void);
extern struct cmd* cmd_pop_ack(u32 seq);

void conn_free(struct conn *c) {
    msf_del_event(rpc->rx_ep_fd, c->fd);
    drain_fd(c->fd, 256);
    sclose(c->fd);
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
    msg->msg_flags = MSG_NOSIGNAL | MSG_WAITALL;//MSG_MORE
    msg->msg_iov = iov;
    msg->msg_iovlen = iovlen;
}

void rx_handle_result(struct conn *c, s32 rc) {

    if (unlikely(0 == rc)) {
        printf("Conn close by peer fd(%d) ret(%d) errno(%d)\n", c->fd, rc, errno);
        conn_free(c);
        c->desc.recv_state = io_close;
    } else if (rc < 0) {
        printf("Conn recv error, fd(%d) ret(%d) errno(%d).\n", c->fd, rc, errno);
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
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
        printf("Close by peer fd(%d) ret(%d) errno(%d).\n", 
            c->fd, rc, errno);
        conn_free(c);
        c->desc.send_state = io_close;
    } else if (rc < 0) {
        printf("Conn send error, fd(%d) ret(%d) errno(%d).\n", c->fd, rc, errno);
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
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

static s32 login_init(void) {

    struct basic_head *bhs = NULL;
    struct login_pdu *login = NULL;
    struct cmd *req_cmd = NULL;

    req_cmd = cmd_new(sizeof(struct login_pdu));
    if (!req_cmd) {
        printf("Login new cmd item fail.\n");
        return -1;
    }

    bhs = &req_cmd->bhs;
    login = (struct login_pdu *)req_cmd->buffer;

    bhs->magic      = RPC_MAGIC;
    bhs->version    = RPC_VERSION;
    bhs->opcode     = RPC_REQ;
    bhs->seq        = time(NULL);
    bhs->timeout    = MSF_NO_WAIT;
    bhs->cmd        = RPC_LOGIN;
    bhs->srcid      = rpc->cid;
    bhs->dstid      = RPC_MSG_SRV_ID;
    bhs->datalen    = sizeof(struct login_pdu);
    memcpy(login->name, rpc->name, min(strlen(rpc->name), sizeof(rpc->name)));
    login->chap = rpc->chap;
    req_cmd->used_len = sizeof(struct login_pdu);
    req_cmd->callback = rpc->req_scb;

    refcount_incr(&req_cmd->ref_cnt);
    cmd_push_tx(req_cmd);

    if (msf_eventfd_notify(rpc->ev_conn.fd) < 0) {
        cmd_free(req_cmd);
        return -1;
    }

    return 0;
}

s32 rx_thread_handle_req_direct(struct conn *c) {

    printf("Rx thread handle ipc request.\n");

    s32 rc = -1;
    s32 req_len = -1;
    struct basic_head *bhs = NULL;
    struct cmd *req = NULL;

    //if (MSF_NO_WAIT == c->bhs.timeout)
    //    return 0;

    req_len = max(c->bhs.restlen, c->bhs.datalen);
    req = cmd_new(req_len);
    if (!req) {
        printf("Rx thread fail to new ack cmd for request.\n");
        return -1;
    }

    memcpy(&req->bhs, &c->bhs, sizeof(struct basic_head));
    bhs = &req->bhs;

    if (rpc->req_scb) {
        rc = rpc->req_scb((s8*)req->buffer, bhs->restlen, bhs->cmd);
        if (rc < 0) {
            cmd_free(req);
            /* Rsp error code */
            return -1;
        }
    }

    bhs->srcid = bhs->srcid^bhs->dstid;
    bhs->dstid = bhs->srcid^bhs->dstid;
    bhs->srcid = bhs->srcid^bhs->dstid; 
    bhs->opcode = RPC_ACK;
    bhs->datalen = bhs->restlen;
    bhs->restlen = 0;
    req->used_len = bhs->datalen;

    refcount_incr(&req->ref_cnt);
    cmd_push_tx(req);
    msf_eventfd_notify(rpc->ev_conn.fd);

    return 0;
}

s32 rx_thread_handle_req_data(struct conn *c, struct cmd *new_cmd) {

    return 0;
}

s32 rx_thread_handle_ack_data(struct conn *c, struct cmd *ack_cmd) {

    printf("Rx thread handle ipc ack.\n");

    s32 rc = -1;
    struct basic_head *bhs = NULL;

    bhs = &c->bhs;

    ack_cmd->bhs.errcode = bhs->errcode;
        
    if (MSF_NO_WAIT == bhs->timeout) {
        if (rpc->ack_scb) {
            rpc->ack_scb(ack_cmd->buffer, bhs->datalen, bhs->cmd);
        }
        /* No need ack, cmd has been freed.*/
        return 0;
    }

    printf("Recv IPC_ACK ref_count(%d) errcode(%d).\n", ack_cmd->ref_cnt, bhs->errcode);

    if (ack_cmd->ref_cnt <= 0) {
        /* if cmd is timeout, put it to free list.*/
        //cmd_free(ack_cmd);
        //return -1;
    }
    sem_post(&ack_cmd->ack_sem);
    return 0;
}

s32 rx_thread_handle_ack_direct(struct conn *c) {

    printf("Rx thread handle ipc ack.\n");

    s32 rc = -1;
    struct basic_head *bhs = NULL;
    struct cmd* ack_item = NULL;

    bhs = &c->bhs;

    if (unlikely(RPC_MSG_SRV_ID == bhs->srcid)) {
        if (RPC_LOGIN == bhs->cmd) {
            printf("Notify login rsp code(%d).\n", bhs->errcode);
            if (unlikely(RPC_LOGIN_FAIL == bhs->errcode)) {
               rpc->state = rpc_uninit;
            } else {
               rpc->state = rpc_inited;
            }
        }
    }

    if (MSF_NO_WAIT == bhs->timeout) {
       /* No need syn ack, cmd has been freed, 
        * get a new cmd to handle asyn ack.*/
       return 0;
    }

    /* Find cmd in ack list by uni syn*/ 
    ack_item = cmd_pop_ack(bhs->seq);
    if (!ack_item) return -1;

    ack_item->bhs.errcode = bhs->errcode;

    printf("Recv IPC_ACK ref_count(%d) errcode(%d).\n", ack_item->ref_cnt, bhs->errcode);

    if (ack_item->ref_cnt <= 0) {
        /* if cmd is timeout, service interface has free cmd,
         * here only put it to free list.*/
        cmd_free(ack_item);
        return -1;
    }

    if (unlikely(RPC_EXEC_SUCC != bhs->errcode)) {
        printf("Rx ack cmd(%u) server notify error(%u).\n", bhs->cmd, bhs->errcode);
        sem_post(&ack_item->ack_sem);
        return -1;
    }

    if (rpc->ack_scb) {
        rpc->ack_scb(ack_item->buffer, ack_item->used_len, bhs->cmd);
    }
    sem_post(&ack_item->ack_sem);
    return rc;
}

static inline void rx_thread_map_bhs(struct conn *c) {

    c->desc.rx_iosize = 0;
    c->desc.rx_iovcnt = 0;
    
    if (unlikely(io_read_half == c->desc.recv_state)) {
        conn_add_rx_iovec(c, &c->bhs + c->desc.rx_iovoffset, 
            sizeof(struct basic_head)-c->desc.rx_iovoffset);
    } else {
        c->desc.rx_iovused = 0;
        c->desc.rx_iovoffset = 0;
        c->desc.recv_stage = stage_recv_bhs;
        msf_memzero(&c->bhs, sizeof(struct basic_head));
        conn_add_rx_iovec(c, &c->bhs, sizeof(struct basic_head));
    } 

    init_msghdr(&c->desc.rx_msghdr, c->desc.rx_iov, c->desc.rx_iovcnt);
}


static inline void rx_thread_map_data(struct conn *c) {

    struct cmd *curr_cmd = NULL;
    c->desc.rx_iosize = 0;
    c->desc.rx_iovcnt = 0;

    curr_cmd = (struct cmd *)c->curr_recv_cmd;
    if (unlikely(io_read_half == c->desc.recv_state)) {
       conn_add_rx_iovec(c, (s8*)curr_cmd->buffer + c->desc.rx_iovoffset,
           curr_cmd->used_len - c->desc.rx_iovoffset);
    } else {
       c->desc.rx_iovused = 0;
       c->desc.rx_iovoffset = 0;
       conn_add_rx_iovec(c, curr_cmd->buffer, curr_cmd->used_len);
    } 

    init_msghdr(&c->desc.rx_msghdr, c->desc.rx_iov, c->desc.rx_iovcnt);
}

static inline void tx_thread_map(struct conn *c, struct cmd *tx_cmd) {

    if (unlikely(io_write_half == c->desc.send_state)) {

        /*Check if bhs has been send*/
        if (c->desc.tx_iovoffset >= sizeof(struct basic_head)) {
            conn_add_tx_iovec(c, (s8*)tx_cmd->buffer + 
                c->desc.tx_iovoffset - sizeof(struct basic_head), 
                tx_cmd->used_len - (c->desc.tx_iovoffset-sizeof(struct basic_head)));
        } else {
            conn_add_tx_iovec(c, &tx_cmd->bhs + c->desc.tx_iovoffset, 
                sizeof(struct basic_head) - c->desc.tx_iovoffset);
            conn_add_tx_iovec(c, tx_cmd->buffer, tx_cmd->used_len);
        }
    } else {
        c->desc.send_state = io_init;
        c->desc.tx_iosize = 0;
        c->desc.tx_iovcnt = 0;
        c->desc.tx_iovused = 0;
        c->desc.tx_iovoffset = 0;
        conn_add_tx_iovec(c, &tx_cmd->bhs, sizeof(struct basic_head));
        conn_add_tx_iovec(c, tx_cmd->buffer, tx_cmd->used_len);
    }

    init_msghdr(&c->desc.tx_msghdr, c->desc.tx_iov, c->desc.tx_iovcnt);

}

static void rx_thread_handle_bhs(struct conn *c) {

    struct cmd *new_cmd = NULL;
    struct cmd *ack_cmd = NULL;
    struct basic_head *bhs = NULL;

    bhs = &c->bhs;

    printf("\n");
    printf("###################################\n");
    printf("bhs:\n");
    printf("bhs version:0x%x\n",    bhs->version);
    printf("bhs magic:0x%x\n",      bhs->magic);
    printf("bhs srcid:0x%x\n",      bhs->srcid);
    printf("bhs dstid:0x%x\n",      bhs->dstid);
    printf("bhs opcode:%u\n",       bhs->opcode);
    printf("bhs cmd:%u\n",          bhs->cmd);
    printf("bhs seq:%u\n",          bhs->seq);
    printf("bhs errcode:%u\n",      bhs->errcode);
    printf("bhs datalen:%u\n",      bhs->datalen);
    printf("bhs restlen:%u\n",      bhs->restlen);
    printf("bhs checksum:%u\n",     bhs->checksum);
    printf("bhs timeout:%u\n",      bhs->timeout);
    printf("###################################\n");
    printf("\n");

    if (unlikely(RPC_VERSION != bhs->version || RPC_MAGIC != bhs->magic)) {
        printf("Notify login rsp code(%d).\n", bhs->errcode);
        conn_free(c);
        return ;
    }

    /* If bhs no data, handle bhs, otherwise goto recv data*/
    if (0 == bhs->datalen) {
        if (RPC_REQ == bhs->opcode) {
            printf("Direct handle req.\n");
            rx_thread_handle_req_direct(c);
        } else if (RPC_ACK == bhs->opcode) {
            printf("Direct handle ack.\n");
            rx_thread_handle_ack_direct(c);
        }
        c->desc.recv_stage = stage_recv_next;
    } else {
        c->desc.recv_stage = stage_recv_data;
        printf("Need to recv extra payload.\n");

        if (bhs->opcode == RPC_ACK && bhs->timeout != MSF_NO_WAIT) {
            /* Find cmd in ack list by uni syn*/ 
            ack_cmd = cmd_pop_ack(bhs->seq);
            if (!ack_cmd) return;

            if (bhs->datalen > ack_cmd->total_len) {
                printf("Ack datalen(%u) not equal bhs datelen(%u).\n",
                    ack_cmd->used_len, bhs->datalen);
            }
            ack_cmd->used_len = bhs->datalen;
            c->curr_recv_cmd = ack_cmd;
        } else {
            new_cmd = cmd_new(bhs->datalen);
            if (!new_cmd) return;
            
            new_cmd->used_len = bhs->datalen;
            c->curr_recv_cmd = new_cmd;
        }
    }
    return;
}

static void rx_thread_read_bhs(struct conn *c) {

    s32 rc = -1;

    if (unlikely(io_close == c->desc.recv_state)) {
        return;
    } else {
        rx_thread_map_bhs(c);
    }

    rc = msf_recvmsg(c->fd, &c->desc.rx_msghdr);

    rx_handle_result(c, rc);

    if (io_read_done == c->desc.recv_state) {
        rx_thread_handle_bhs(c);
    }
    return;
}

static void rx_thread_read_data(struct conn *c) {

    s32 rc = -1;
    struct basic_head *bhs = NULL;

    if (unlikely(io_close == c->desc.recv_state)) {
        return;
    } else {
        rx_thread_map_data(c);
    }

    bhs = &c->bhs;

    printf("Read data len is %u.\n", bhs->datalen);

    rc = msf_recvmsg(c->fd, &c->desc.rx_msghdr);

    printf("Recv data len is %u.\n", rc);

    rx_handle_result(c, rc);

    if (io_read_done == c->desc.recv_state) {
        if (RPC_REQ == bhs->opcode) {
            printf("Direct handle req with data.\n");
            rx_thread_handle_req_data(c, c->curr_recv_cmd);
        } else if (RPC_ACK == bhs->opcode) {
            printf("Direct handle ack with data.\n");
            if (bhs->timeout != MSF_NO_WAIT)
                rx_thread_handle_ack_data(c, c->curr_recv_cmd);
            else
                rx_thread_handle_ack_data(c, c->curr_recv_cmd);
        }
        c->desc.recv_stage = stage_recv_next;
    }
    return;
}

static void rx_thread_loop(struct conn *c) {

    s32 rc = -1;
    
    do {
        switch (c->desc.recv_stage) {
            case stage_recv_bhs:
                printf("Stage to recv bhs.\n");
                rx_thread_read_bhs(c);
                if (unlikely(io_read_half == c->desc.recv_state
                    || io_close == c->desc.recv_state)) {
                    /* half recv, must return to recv again */
                    return;
                }
                break;
            case stage_recv_data:
                printf("Stage to recv data.\n");
                rx_thread_read_data(c);
                if (unlikely(io_read_half == c->desc.recv_state || 
                    io_close == c->desc.recv_state)) {
                    return;
                }
                break;
            case stage_recv_next:
                printf("Stage to recv next.\n");
                rx_thread_map_bhs(c);
                return;
            default:
                printf("Stage default now.\n");
                return;
        }
    } while(1);
    return;
}

void * rx_thread_worker(void *lparam) {

    struct client *rpc = (struct client*)lparam;
    struct conn *c;
    s32 idx = -1;
    s32 nfds = invalid_socket;
    struct epoll_event events[max_epoll_event];

    while (!rpc->flags) {

        nfds = epoll_wait(rpc->rx_ep_fd, events, max_epoll_event, -1);
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
                //printf("RX thread read fd(%d) idx(%d).\n", c->fd, idx);
                rx_thread_loop(c);
            } 
        }
    }
    printf("RX thread will exit now.\n");
    return NULL;
}

static void tx_thread_loop(struct conn *c) {

    s32 rc = -1;
    struct cmd *tx_cmd = NULL;
    struct basic_head *bhs = NULL;

    msf_eventfd_clear(c->fd);

    tx_cmd = cmd_pop_tx();
    if (unlikely(!tx_cmd)) {
        fprintf(stderr, "TX fail to pop one write buffer item.\n");
        return;
    }

    fprintf(stderr, "TX item ref count(%d).\n", tx_cmd->ref_cnt);

    if (refcount_decr(&tx_cmd->ref_cnt) <= 0) {
        fprintf(stderr, "refcount_decr: %d\n", tx_cmd->ref_cnt);
        cmd_free(tx_cmd);
        return;
    }

    fprintf(stderr, "TX item ref count(%d) req used_len(%d).\n", 
        tx_cmd->ref_cnt, tx_cmd->used_len);

    bhs = (struct basic_head*)&tx_cmd->bhs;

    tx_thread_map(c, tx_cmd);

    rc = msf_sendmsg(rpc->cli_conn.fd, &c->desc.tx_msghdr);
    tx_handle_result(c, rc);
    if (unlikely(io_close == c->desc.send_state)) {
        fprintf(stderr, "TX sendmsg not done, ret(%d).\n", rc);
        cmd_free(tx_cmd);
        return;
    } else if (unlikely(io_write_half == c->desc.send_state)) {
        cmd_push_tx_head(tx_cmd);
        return;
    }

    printf("TX sendmsg fd(%d) ret(%d)(%d).\n", rpc->cli_conn.fd, rc, tx_cmd->used_len);

    if (MSF_NO_WAIT == bhs->timeout || RPC_ACK == bhs->opcode) {
        fprintf(stderr, "No need recv ack, free node, del key.\n");
        cmd_free(tx_cmd);
    } else {
        fprintf(stderr, "Need recv ack, push into ack_queue.\n");
        cmd_push_ack(tx_cmd);
    }
}

void * tx_thread_worker(void *lparam) {

    struct client *rpc = (struct client*)lparam;
    struct conn *c = NULL;

    s32 idx = -1;
    s32 nfds = invalid_socket;
    struct epoll_event events[max_epoll_event];

    while (!rpc->flags) {

        nfds = epoll_wait(rpc->ev_ep_fd, events, max_epoll_event, -1);
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
                printf("TX thread event fd(%d) idx(%d).\n", c->fd, idx);
                tx_thread_loop(c);
            } 
        }
    }
    return NULL;
}

s32 thread_init(void) {

    s32 rc = -1;

    rc = pthread_spawn(&rpc->tx_tid, (void*)tx_thread_worker, rpc);
    if (rc < 0) {
        printf("TX thread create failed, ret(%d), errno(%d).\n", rc, errno);
        return -1;
    }

    rc = pthread_spawn(&rpc->rx_tid, (void*)rx_thread_worker, rpc);
    if (rc < 0) {
        printf("RX thread create failed, ret(%d), errno(%d).\n", rc, errno);
        return -1;
    }

    rc = login_init();
    if (rc < 0) {
        printf("Add login request failed, ret(%d), errno(%d).\n", rc, errno);
        return -1;
    }

    usleep(500);

    return 0;
}


