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

s32 network_init(void);
void network_deinit(void);

s32 network_init(void) {

    s16 event = EPOLLIN;

    /**/
#ifdef EPOLLEXCLUSIVE
    event |= EPOLLEXCLUSIVE;
#endif

    if (srv->unix_enable) {
        srv->unix_socket = msf_server_unix_socket(srv->backlog, srv->unix_path, 0777);
        if (srv->unix_socket < 0) return -1;

        srv->listen_unix = conn_new(srv->unix_socket, event);
        if (!srv->listen_unix) return -1;
    }

    if (srv->net_enable_v4) {
        srv->net_socket_v4 = msf_server_socket(srv->serv_node_v4, srv->net_prot_v4, 
                                        srv->tcp_port, srv->backlog);
        if (srv->net_socket_v4 < 0) return -1;
        
        srv->listen_net_v4 = conn_new(srv->net_socket_v4, event);
        if (!srv->listen_net_v4) return -1;
    }

    if (srv->net_enable_v6) {
        srv->net_socket_v6 = msf_server_socket(srv->serv_node_v6, srv->net_prot_v6, 
                                        srv->tcp_port, srv->backlog);
        if (srv->net_socket_v6 < 0) return -1;
        
        srv->listen_net_v6 = conn_new(srv->net_socket_v6, event);
        if (!srv->listen_net_v6) return -1;
    }

    return 0;
}

void network_deinit(void) {

    sclose(srv->unix_socket);
    sclose(srv->net_socket_v4);
    sclose(srv->net_socket_v6);
}
