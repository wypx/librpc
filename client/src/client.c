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

struct client rpc_param;
struct client *rpc = &rpc_param;

extern s32 cmd_init(void);
extern void cmd_deinit(void);

extern struct cmd *cmd_new(s32 data_len);
extern void cmd_free(struct cmd *old_cmd);
extern void cmd_push_tx(struct cmd *tx_cmd);
extern void cmd_push_ack(struct cmd *ack_cmd);;
extern struct cmd* cmd_pop_tx(void);
extern struct cmd* cmd_pop_ack(void);
extern s32 thread_init(void);

s32 signal_init(void) {
    signal_handler(SIGHUP,	SIG_IGN);
    signal_handler(SIGTERM, SIG_IGN);
    signal_handler(SIGPIPE, SIG_IGN);
    return 0;
}

s32 network_init(void) {

    s32 rc = -1;
    s16 event = EPOLLIN;

    struct conn *clic = NULL;
    struct conn *evc = NULL;

    clic = &rpc->cli_conn;
    evc = &rpc->ev_conn;

    msf_memzero(&clic->desc, sizeof( struct conn_desc));
    
    clic->desc.recv_stage = stage_recv_bhs;
    clic->desc.send_stage  = stage_send_bhs;

    evc->desc.recv_stage = stage_recv_bhs;
    evc->desc.send_stage  = stage_send_bhs;

    if (0 == msf_strncmp(rpc->server_host, local_host_v4, strlen(local_host_v4))
     || 0 == msf_strncmp(rpc->server_host, local_host_v6, strlen(local_host_v6))) {
        if (rpc->cid == RPC_UPNP_ID)
            clic->fd = msf_connect_to_unix_socket(MSF_RPC_UNIX_UPNP, MSF_RPC_UNIX_SERVER);
        else
            clic->fd = msf_connect_to_unix_socket(MSF_RPC_UNIX_DLNA, MSF_RPC_UNIX_SERVER);
    } else {
        clic->fd = msf_connect_to_host(rpc->server_host, rpc->server_port);
    }

    if (clic->fd < 0) {
        return -1;
    }

    evc->fd = msf_eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC | EFD_SEMAPHORE);
    if (evc->fd < 0) {
        return -1;
    }


    MSF_RPC_LOG(DBG_INFO,  "Network init client fd(%u)", clic->fd);
    MSF_RPC_LOG(DBG_INFO,  "Network init event fd(%u)", evc->fd);

    usleep(500);
    
    return 0;
}

s32 client_init(s8 *name, s8 *host, s8 *port, srvcb req_scb, srvcb ack_scb) {

    if (unlikely(!name || !host || !port || !req_scb || !ack_scb)) {
        MSF_RPC_LOG(DBG_ERROR, "RPC client init param invalid");
        return -1;
    }

    s8 log_path[256] = { 0 };

    snprintf(log_path, sizeof(log_path)-1, RPC_LOG_FILE_PATH, name);
    log_init(log_path);

    msf_memzero(rpc, sizeof(struct client));
    rpc->state = rpc_uninit;
    rpc->rx_tid = ~0;
    rpc->tx_tid = ~0;
    if (strstr(name, "UPNP")) {
        rpc->cid = RPC_UPNP_ID;
    } else if (strstr(name, "DLNA")) {
        rpc->cid = RPC_DLNA_ID;
    }
    
    rpc->req_scb = req_scb;
    rpc->ack_scb = ack_scb;
    memcpy(rpc->name, name, min(strlen(name), sizeof(rpc->name)));
    memcpy(rpc->server_host, host, min(strlen(host), sizeof(rpc->server_host)));
    memcpy(rpc->server_port, port, min(strlen(port), sizeof(rpc->server_port)));

    if (msf_timer_init() < 0) goto error;

    if (signal_init() < 0) goto error;

    MSF_RPC_LOG(DBG_INFO, "Client signal init successful");

    if (cmd_init() < 0) goto error;
    
    MSF_RPC_LOG(DBG_INFO, "Client cmd init successful");

    if (network_init() < 0)   goto error;

    MSF_RPC_LOG(DBG_INFO, "Client network init successful");

    if (thread_init() < 0) 	 goto error;

    MSF_RPC_LOG(DBG_INFO, "Client thread init successful");

    rpc->state = rpc_inited;

    return 0;
error:
    rpc->state = rpc_uninit;
    client_deinit();
    return -1;
}

s32 client_deinit(void) {

    msf_timer_destroy();

    cmd_deinit();

    msf_drain_fd(rpc->ev_conn.fd, 256);
    msf_drain_fd(rpc->cli_conn.fd, 256);

    sclose(rpc->ev_conn.fd);
    sclose(rpc->cli_conn.fd);

    msf_event_base_loop_break(rpc->ev_rx_base);
    msf_event_base_loop_break(rpc->ev_tx_base);

    rpc->state = rpc_uninit;

    return 0;
}

s32 client_service(struct basic_pdu *pdu) {

    struct basic_head *bhs = NULL;
    struct cmd *new_cmd = NULL;

    if (rpc->state != rpc_inited) {
        MSF_RPC_LOG(DBG_INFO, "client_state: %d", rpc->state);
        return -1;
    }

    MSF_RPC_LOG(DBG_INFO, "Call service head len(%lu) payload(%u) cmd(%u)", 
        sizeof(struct basic_head), pdu->paylen, pdu->cmd);

    new_cmd = cmd_new(max(pdu->paylen, pdu->restlen));
    if (!new_cmd) return -1;

    bhs = &new_cmd->bhs;
    bhs->magic   = RPC_MAGIC;
    bhs->version = RPC_VERSION;
    bhs->opcode  = RPC_REQ;
    bhs->datalen = pdu->paylen;
    bhs->restlen = pdu->restlen;

    bhs->srcid =  rpc->cid;
    bhs->dstid =  pdu->dstid;
    bhs->cmd =  pdu->cmd;
    bhs->seq = time(NULL);
    bhs->checksum = 0;
    bhs->timeout  = pdu->timeout;

    new_cmd->used_len = pdu->paylen; 
    new_cmd->callback = rpc->req_scb;

    if (pdu->paylen > 0) {
        memcpy((s8*)new_cmd->buffer, pdu->payload, pdu->paylen);
    }

    refcount_incr(&new_cmd->ref_cnt);
    cmd_push_tx(new_cmd);

    if (msf_eventfd_notify(rpc->ev_conn.fd) < 0) {
        cmd_free(new_cmd);
        return -1;
    }

    if (MSF_NO_WAIT != pdu->timeout) {

        sem_init(&(new_cmd)->ack_sem, 0, 0);
        /* Check what happened */
        if (-1 == sem_wait_i(&(new_cmd->ack_sem), pdu->timeout)) {
            MSF_RPC_LOG(DBG_ERROR, "Wait for service ack timeout(%u)", pdu->timeout);
            sem_destroy(&(new_cmd)->ack_sem);
            cmd_free(new_cmd);
            return -2;
        } 

        MSF_RPC_LOG(DBG_INFO, "Notify peer errcode[%d]", bhs->errcode);

        if (likely(RPC_EXEC_SUCC == bhs->errcode)) {
            memcpy(pdu->restload, (s8*)new_cmd->buffer, pdu->restlen);
        }

        sem_destroy(&new_cmd->ack_sem);
        cmd_free(new_cmd);
    }
    
    return bhs->errcode;
}

