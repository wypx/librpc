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

#include <server.h>

static struct server g_server;
struct server *srv = &g_server;

extern s32 config_init(void);
extern void config_deinit(void);

extern s32 conn_init(void);
extern void conn_deinit(void);

extern s32 cmd_init(void);
extern void cmd_deinit(void);
extern struct cmd *cmd_new(void);

extern s32 thread_init(void);
extern void thread_deinit(void);
extern s32 network_init(void);
extern void network_deinit(void);

static void signal_init(void);
static void param_init(void);

void param_init(void) {

    srv->pid = getpid();
    srv->pid_file = default_pid_path;
    srv->conf_file = default_config_path;
    srv->log_file = default_log_path;

    srv->daemon = false;
    srv->verbose = true;

    srv->backlog = max_listen_backlog;
    srv->unix_enable = true;
    srv->unix_socket = invalid_socket;
    srv->access_mask = 0777;
    srv->unix_path = default_unix_path;

    srv->net_enable_v4 = false;
    srv->net_socket_v4 = invalid_socket;
    srv->net_prot_v4 = IPPROTO_TCP;
    srv->net_enable_v6 = false;
    srv->net_socket_v6 = invalid_socket;
    srv->net_prot_v6 = IPPROTO_TCP;
    srv->tcp_port = 9999;
    srv->udp_port = 8888;
    srv->serv_node_v4 = "192.168.58.133";
    srv->serv_node_v6 = NULL;

    srv->pack_type = packet_binary;

    srv->max_cmd = max_conn_cmd_num;
    srv->max_bytes = 64 * 1024 * 1024; /* default is 64MB */
    srv->max_conns = max_conn_number;/* to limit connections-related memory to about 5MB */

    srv->max_cores = get_nprocs_conf();
    srv->used_cores = get_nprocs();
    srv->max_thread = srv->used_cores - 1;

    pthread_mutex_init(&srv->init_lock, NULL);
    pthread_cond_init(&srv->init_cond, NULL);
}

void signal_init(void) {

    signal_handler(SIGHUP,  SIG_IGN);
    signal_handler(SIGTERM, SIG_IGN);
    signal_handler(SIGPIPE, SIG_IGN);

    /* if we want to ensure our ability to dump core, don't chdir to / */
    if (srv->daemon) {
         if (daemonize(0, srv->verbose) < 0) {
             MSF_AGENT_LOG(DBG_ERROR, "Failed to daemon() in order to daemonize.");
        }
    }
};

s32 server_init(void) {

    s32 rc;

    param_init();

    if (msf_os_init() < 0)
        return -1;

    MSF_AGENT_LOG(DBG_INFO, "Server os init successful.");

    if (config_init() < 0) goto exit;

    MSF_AGENT_LOG(DBG_INFO, "Server config init successful.");

    save_pid(srv->pid_file);

    signal_init();

    MSF_AGENT_LOG(DBG_INFO, "Server signal init successful.");

    if (cmd_init() < 0) goto exit;

    MSF_AGENT_LOG(DBG_INFO, "Server cmd init successful.");

    if (conn_init() < 0) goto exit;

    MSF_AGENT_LOG(DBG_INFO, "Server conn init successful.");

    if (thread_init() < 0) goto exit;

    MSF_AGENT_LOG(DBG_INFO, "Server thread init successful.");

    if (network_init() < 0) goto exit;

    MSF_AGENT_LOG(DBG_INFO, "Server network init successful.");

    /* Give the sockets a moment to open. I know this is dumb, but the error
     * is only an advisory.
     */
    usleep(1000);

    return 0;
exit:
    server_deinit();
    return -1;
}

void server_deinit(void) {
    remove_pidfile(srv->pid_file);
    config_deinit();
    cmd_deinit();
    conn_deinit();
    thread_deinit();
    network_deinit();
    return;
}

