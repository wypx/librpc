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
extern void thread_deinit(void);

void sig_handler(s32 sig) {
 
    switch (sig) {
        case SIGBUS:
            MSF_RPC_LOG(DBG_ERROR, "Got sigbus error.");
            raise(SIGKILL);
            break;
         case SIGSEGV:
            MSF_RPC_LOG(DBG_ERROR, "Got sigsegv error.");
            break;
          case SIGILL:
            MSF_RPC_LOG(DBG_ERROR, "Got sigill error.");
            break;
          default:
            break;
    }
}

s32 signal_init(void) {

    msf_enable_coredump();

    signal_handler(SIGHUP,  SIG_IGN);
    signal_handler(SIGTERM, SIG_IGN);
    signal_handler(SIGPIPE, SIG_IGN);
    signal_handler(SIGBUS, sig_handler);
    signal_handler(SIGSEGV, sig_handler);
    signal_handler(SIGILL, sig_handler);

    return 0;
}

s32 cli_event_init(void) {

    struct conn *evc = NULL;

    evc = &rpc->evt_conn;
    
    evc->desc.recv_stage = stage_recv_bhs;
    evc->desc.send_stage  = stage_send_bhs;

    evc->fd = msf_eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC | EFD_SEMAPHORE);
    if (evc->fd < 0) {
        return -1;
    }

    MSF_RPC_LOG(DBG_INFO,  "Network init event fd(%u).", evc->fd);

    usleep(500);
    
    return 0;
}

s32 cli_log_init(void)
{
    s32 rc;
    s8 log_path[256] = { 0 };
    s8 proc_name[256] = { 0 };

    rc = msf_get_process_name(proc_name);
    if (rc != MSF_OK) {
        return MSF_ERR;
    }

    msf_snprintf(log_path, sizeof(log_path)-1,
        RPC_LOG_FILE_PATH, proc_name);
    rc= msf_log_init(log_path);
    if (rc != MSF_OK) {
        return MSF_ERR;
    }

    return MSF_OK;
}

extern void rx_thread_callback(void *arg);
extern s32 login_init(void);
s32 cli_connect_agent(void)
{
    s32 rc = -1;
    s16 event = EPOLLIN;
    s8 unix_path[128] = { 0 };

    struct conn *clic = NULL;

    clic = &rpc->cli_conn;

    msf_memzero(&clic->desc, sizeof( struct conn_desc));
    
    clic->desc.recv_stage = stage_recv_bhs;
    clic->desc.send_stage  = stage_send_bhs;

    if (0 == msf_strncmp(rpc->srv_host, LOCAL_HOST_V4, strlen(LOCAL_HOST_V4))
     || 0 == msf_strncmp(rpc->srv_host, LOCAL_HOST_V4, strlen(LOCAL_HOST_V6))) {
        msf_snprintf(unix_path, sizeof(unix_path) - 1, 
            MSF_RPC_UNIX_FORMAT, rpc->cli_conn.name);
        clic->fd = msf_connect_unix_socket(unix_path, MSF_RPC_UNIX_SERVER);
    } else {
        clic->fd = msf_connect_host(rpc->srv_host, rpc->srv_port);
    }

    if (clic->fd < 0) {
        return -1;
    }

    MSF_RPC_LOG(DBG_INFO,  "Network init client fd(%u).", clic->fd);

    rpc->rx_evt = msf_event_new(rpc->cli_conn.fd, rx_thread_callback,
                NULL, NULL, &rpc->cli_conn);
    if (!rpc->rx_evt) {
        return -1;
    }
    msf_event_add(rpc->rx_evt_base, rpc->rx_evt);

    rc = login_init();
    if (rc < 0) {
        MSF_RPC_LOG(DBG_ERROR, "Add login request failed, ret(%d), errno(%d).", rc, errno);
        return -1;
    }
    return 0;
}

s32 cli_check_param(struct client_param *param)
{
    if (!param) {
        MSF_RPC_LOG(DBG_ERROR, "Cli param pointer invalid.");
        return -1;
    }

    if (!param->name) {
        MSF_RPC_LOG(DBG_ERROR, "Cli param name invalid.");
        return -1;
    }

    if (!param->host) {
        MSF_RPC_LOG(DBG_ERROR, "Cli param host invalid.");
        return -1;
    }

    if (!param->port) {
        MSF_RPC_LOG(DBG_ERROR, "Cli param port invalid.");
        return -1;
    }

    if (!param->req_scb) {
        MSF_RPC_LOG(DBG_ERROR, "Cli param seq cb invalid.");
        return -1;
    }

    if (!param->ack_scb) {
        MSF_RPC_LOG(DBG_ERROR, "Cli param ack cb invalid.");
        return -1;
    }

    return 0;
}

void cli_param_init(struct client_param *param)
{
    msf_memzero(rpc, sizeof(struct client));
    rpc->state = rpc_uninit;
    rpc->rx_tid = ~0;
    rpc->tx_tid = ~0;
    rpc->cli_conn.cid = param->cid;
    rpc->req_scb = param->req_scb;
    rpc->ack_scb = param->ack_scb;
    msf_memcpy(rpc->cli_conn.name, param->name,
        min(msf_strlen(param->name), (size_t)MAX_CONN_NAME));
    msf_memcpy(rpc->srv_host, param->host,
        min(msf_strlen(param->host), sizeof(rpc->srv_host)));
    msf_memcpy(rpc->srv_port, param->port,
        min(msf_strlen(param->port), sizeof(rpc->srv_port)));
}

s32 cli_timer_thrd(void *arg) {
    
    struct timeval newtime, difference;
    gettimeofday(&newtime, NULL);
    timersub(&newtime, &rpc->lasttime, &difference);
    double  elapsed = difference.tv_sec + (difference.tv_usec / 1.0e6);

    //client_reg_heart(RPC_HEARTBEAT);

    MSF_RPC_LOG(DBG_DEBUG,
            "Cli timer called at %d: %.3f seconds elapsed.",
            (s32)newtime.tv_sec, elapsed);
    
    if (rpc->state == rpc_uninit) {
        MSF_RPC_LOG(DBG_DEBUG, "Cli ready reconnect to server...");
        if (cli_connect_agent() == 0) {
            rpc->state = rpc_inited;
        }
    }

    rpc->lasttime = newtime;

    return -1;
}

s32 client_agent_init(struct client_param *param)
{
    s32 rc;

    if (unlikely(cli_check_param(param) < 0)) {
        return -1;
    }

    cli_param_init(param);

    if (cli_log_init() < 0) {
        return -1;
    }

    if (signal_init() < 0) goto error;

    if (cmd_init() < 0) goto error;

    if (cli_event_init() < 0) goto error;

    if (thread_init() < 0) goto error;

    if (cli_connect_agent() < 0) goto error;

    if (msf_timer_init() < 0) goto error;

    msf_timer_add(1, 2000, cli_timer_thrd, rpc, CYCLE_TIMER, 100);

    rpc->state = rpc_inited;

    return 0;
error:
    rpc->state = rpc_uninit;
    client_agent_deinit();
    return -1;
}

s32 client_agent_deinit(void) {

    msf_timer_destroy();
    thread_deinit();
    cmd_deinit();
    rpc->state = rpc_uninit;
    return 0;
}

s32 client_agent_service(struct basic_pdu *pdu) {

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

    bhs->srcid =  rpc->cli_conn.cid;
    bhs->dstid =  pdu->dstid;
    bhs->cmd =  pdu->cmd;
    msf_atomic_fetch_add(&(rpc->cli_conn.msg_seq), 1);
    bhs->seq = rpc->cli_conn.msg_seq;
    bhs->checksum = 0;
    bhs->timeout  = pdu->timeout;

    new_cmd->used_len = pdu->paylen; 
    new_cmd->callback = rpc->req_scb;

    if (pdu->paylen > 0) {
        msf_memcpy((s8*)new_cmd->buffer, pdu->payload, pdu->paylen);
    }

    refcount_incr(&new_cmd->ref_cnt);
    cmd_push_tx(new_cmd);

    if (msf_eventfd_notify(rpc->evt_conn.fd) < 0) {
          MSF_RPC_LOG(DBG_ERROR, "Notofy tx thread faild.");
        cmd_free(new_cmd);
        return -1;
    }

    if (MSF_NO_WAIT != pdu->timeout) {

        msf_sem_init(&new_cmd->ack_sem);
        /* Check what happened */
        if (-1 == msf_sem_wait(&(new_cmd->ack_sem), pdu->timeout)) {
            MSF_RPC_LOG(DBG_ERROR, "Wait for service ack timeout(%u)", pdu->timeout);
            msf_sem_destroy(&(new_cmd)->ack_sem);
            cmd_free(new_cmd);
            return -2;
        } 

        MSF_RPC_LOG(DBG_INFO, "Notify peer errcode[%d]", bhs->errcode);

        if (likely(RPC_EXEC_SUCC == bhs->errcode)) {
            msf_memcpy(pdu->restload, (s8*)new_cmd->buffer, pdu->restlen);
        }

        msf_sem_destroy(&new_cmd->ack_sem);
        cmd_free(new_cmd);
    }
    
    return bhs->errcode;
}

