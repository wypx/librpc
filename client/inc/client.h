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
#include <conn.h>

//http://itindex.net/detail/53322-rpc-%E6%A1%86%E6%9E%B6-bangerlee
/**
 * load balance: retry for timeout
 * client maintain a star mark for server(ip and port)
 * when request fail or timeout, dec the star by one,
 * since the star eaual as zero, add the ip, port, time
 * into the local info, realse the restict some times later
 * 
 * server info can upload to zookeeper, then share in cluster
 * when new a server node, can distribute this to clients.
 */

#define HAVE_GCC_ATOMICS        1
#define MSF_HAVE_EVENTFD        1
#define RPC_LOG_FILE_PATH       "%s.log"

enum CLIENT_STATE  {
    rpc_uninit,
    rpc_inited,
};

enum BUFFER_IDX {
    buff_zero,
    buff_64B,
    buff_128B,
    buff_256B,
    buff_512B,
    buff_1K,
    buff_2K,
    buff_4K,
    buff_8K,
    buff_max = 12,
} ;

enum cluster_role {
    cluster_invalid = 0,
    cluster_master  = 1,
    cluster_slaver  = 2,
};

struct cluster_node {
    u32 node_id;
    s8  node_name[32];
    u32 node_state;
    u32 node_role;

    s8 node_ipv4[64];
    s8 node_ipv6[64];
};

struct client {
    u32     stop_flag;
    u32     state;
    srvcb   req_scb;
    srvcb   ack_scb;

    s8      srv_host[32];
    s8      srv_port[32];

    struct conn evt_conn;
    struct conn cli_conn;

    s32     timer_fd;
    s32     signal_fd;
    
    pthread_t rx_tid;
    pthread_t tx_tid;
    struct msf_event_base *rx_evt_base;
    struct msf_event_base *tx_evt_base;
    struct msf_event *rx_evt;
    struct msf_event *tx_evt;

    pthread_spinlock_t free_cmd_lock[buff_max];
    struct list_head free_cmd_list[buff_max];

    struct timeval lasttime;
};

extern struct client *rpc;
#define MSF_RPC_LOG(level, ...) \
    msf_log_write(level, rpc->cli_conn.name, MSF_FUNC_FILE_LINE, __VA_ARGS__)

struct client_param {
    u32 cid;
    s8 *name;
    s8 *host;
    s8 *port;
    srvcb req_scb;
    srvcb ack_scb;
};

s32 client_agent_init(struct client_param *param);
s32 client_agent_deinit(void);
s32 client_agent_service(struct basic_pdu *pdu);

