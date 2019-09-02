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

#include <server.h>

extern struct conn *conn_new(s32 new_fd, s32 event);

s32 network_listen_switch(s32 backlog) {

    s32 rc = MSF_ERR;

    if (srv->unix_enable) {
        rc = msf_socket_listen(srv->unix_socket, backlog);
        if (rc < 0) return MSF_ERR;
    }

    if (srv->net_enable_v4) {
        rc = msf_socket_listen(srv->net_socket_v4, backlog);
        if (rc < 0) return MSF_ERR;
    }

    if (srv->net_enable_v6) {
        rc = msf_socket_listen(srv->net_socket_v6, backlog);
        if (rc < 0) return MSF_ERR;
    }
    return MSF_OK;
}

s32 network_init(void) {

    s16 event = EPOLLIN;

    /* 解决惊群 */
#ifdef EPOLLEXCLUSIVE
    event |= EPOLLEXCLUSIVE;
#endif

    if (srv->unix_enable) {
        srv->unix_socket = msf_server_unix_socket(srv->backlog, srv->unix_path, 0777);
        if (srv->unix_socket < 0) return MSF_ERR;

        srv->listen_unix = conn_new(srv->unix_socket, event);
        if (!srv->listen_unix) return MSF_ERR;
    }

    if (srv->net_enable_v4) {
        srv->net_socket_v4 = msf_server_socket(srv->serv_node_v4, srv->net_prot_v4, 
                                        srv->tcp_port, srv->backlog);
        if (srv->net_socket_v4 < 0) return MSF_ERR;
        
        srv->listen_net_v4 = conn_new(srv->net_socket_v4, event);
        if (!srv->listen_net_v4) return MSF_ERR;
    }

    if (srv->net_enable_v6) {
        srv->net_socket_v6 = msf_server_socket(srv->serv_node_v6, srv->net_prot_v6, 
                                        srv->tcp_port, srv->backlog);
        if (srv->net_socket_v6 < 0) return MSF_ERR;
        
        srv->listen_net_v6 = conn_new(srv->net_socket_v6, event);
        if (!srv->listen_net_v6) return MSF_ERR;
    }

    return MSF_OK;
}

void network_deinit(void) {
    sclose(srv->unix_socket);
    sclose(srv->net_socket_v4);
    sclose(srv->net_socket_v6);
}

MSF_STATIC struct  network_ops g_net_ops = {
    .s_sock_init    = network_init,
    .s_sock_deinit  = network_deinit,
    .s_read_cb      = NULL,
    .s_write_cb     = NULL,
    .s_drain_cb     = msf_drain_fd,
    .s_close_cb     = NULL,
    .s_listen_cb    = network_listen_switch,
};

/* Register the ops of network */
MSF_LIBRARY_INITIALIZER(network_pin_ops, 101) {
    srv->net_ops = &g_net_ops;
}


