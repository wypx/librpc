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

MSF_LIBRARY_INITIALIZER(process_init, 101) {

    s8 log_path[256] = { 0 };
    msf_snprintf(log_path, sizeof(log_path)-1, "logger/%s.log", "AGENT");

    if (msf_log_init(log_path) < 0) {
      return;
    }
}

MSF_LIBRARY_INITIALIZER(param_init, 102) {

    srv->pid = msf_getpid();
    srv->pid_file = default_pid_path;
    srv->conf_file = default_config_path;
    srv->log_file = default_log_path;

    srv->daemon = MSF_FALSE;
    srv->verbose = MSF_TRUE;

    srv->backlog = max_listen_backlog;
    srv->unix_enable = MSF_TRUE;
    srv->unix_socket = MSF_INVALID_SOCKET;
    srv->access_mask = 0777;
    srv->unix_path = default_unix_path;

    srv->net_enable_v4 = MSF_FALSE;
    srv->net_socket_v4 = MSF_INVALID_SOCKET;
    srv->net_prot_v4 = IPPROTO_TCP;
    srv->net_enable_v6 = MSF_FALSE;
    srv->net_socket_v6 = MSF_INVALID_SOCKET;
    srv->net_prot_v6 = IPPROTO_TCP;
    srv->tcp_port = 9999;
    srv->udp_port = 8888;
    srv->serv_node_v4 = "192.168.58.133";
    srv->serv_node_v6 = NULL;

    srv->pack_type = packet_binary;

    srv->max_cmd = MAX_CONN_CMD_NUM;
    srv->max_bytes = 64 * 1024 * 1024; /* default is 64MB */
    srv->max_conns = MAX_CONN_NUM;/* to limit connections-related memory to about 5MB */

    srv->max_cores = get_nprocs_conf();
    srv->used_cores = get_nprocs();
    srv->max_thread = srv->used_cores - 1;

    msf_mutex_init(&srv->init_lock);
    msf_cond_init(&srv->init_cond);

    MSF_AGENT_LOG(DBG_DEBUG, "Server param init successful.");
}

MSF_LIBRARY_INITIALIZER(signal_init, 103) {

    signal_handler(SIGHUP,  SIG_IGN);
    signal_handler(SIGTERM, SIG_IGN);
    signal_handler(SIGPIPE, SIG_IGN);

    /* if we want to ensure our ability to dump core, don't chdir to / */
    if (srv->daemon) {
         if (daemonize(0, srv->verbose) < 0) {
             MSF_AGENT_LOG(DBG_ERROR, "Failed to daemon() in order to daemonize.");
        }
    }

    MSF_AGENT_LOG(DBG_DEBUG, "Server signal init successful.");
}

MSF_UNUSED_CHECK s32 server_init(void) {

    s32 rc;

    if (msf_os_init() < 0)
        return -1;

    MSF_AGENT_LOG(DBG_DEBUG, "Server os init successful.");

    if (config_init() < 0) goto exit;

    MSF_AGENT_LOG(DBG_DEBUG, "Server config init successful.");

    msf_create_pidfile(srv->pid_file);
    msf_write_pidfile(srv->pid_file);

    if (cmd_init() < 0) goto exit;

    MSF_AGENT_LOG(DBG_DEBUG, "Server cmd init successful.");

    if (conn_init() < 0) goto exit;

    MSF_AGENT_LOG(DBG_DEBUG, "Server conn init successful.");

    if (thread_init() < 0) goto exit;

    MSF_AGENT_LOG(DBG_DEBUG, "Server thread init successful.");

    rc = srv->net_ops->s_sock_init();
    if (rc < 0) goto exit;

    MSF_AGENT_LOG(DBG_DEBUG, "Server network init successful.");

    /* Give the sockets a moment to open. I know this is dumb, but the error
     * is only an advisory.
     */
    usleep(1000);

    return 0;
exit:
    //server_deinit();
    return -1;
}

MSF_LIBRARY_FINALIZER(server_deinit) {
    msf_delete_pidfile(srv->pid_file);
    config_deinit();
    cmd_deinit();
    conn_deinit();
    thread_deinit();
    srv->net_ops->s_sock_deinit();
    return;
}

