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

#include <msf_event.h>
#include <msf_list.h>
#include <binary.h>

//http://itindex.net/detail/53322-rpc-%E6%A1%86%E6%9E%B6-bangerlee
//超时与重试 -- 考虑负载
/**
 * Client屏蔽策略: 漏桶屏蔽
 * Client对每个后端Server(IP/PORT)维护一个评分
 * 请求失败(失败或者超时), 分数减一, 到分数为0时,
 * 将IP/PORT和时间戳信息记录, 请求消息时候, 跳过它们,
 * 在一段时间后, 再放开限制.
 *
 * 可以把server异常信息上报到中心管理节点(zookeeper),
 * 新增节点的时候, 从中心节点拉取屏蔽信息, 实现集群内信息共享.
 */

#define HAVE_GCC_ATOMICS        1
#define MSF_HAVE_EVENTFD        1

typedef s32 (*srvcb)(s8 *data, u32 len, u32 cmd);

enum auth_state {
    authorized,     /* Client is authorized to access service */
    restricted,     /* Client has limited access to service (bandwidth, ...) */
    refused,        /* Client is always refused to access service (i.e. blacklist) */
} __attribute__((__packed__));

enum client_stat  {
    rpc_uninit,
    rpc_inited,
} __attribute__((__packed__));

struct auth_param {
    s8  user[16];
    s8  pass[16];
    s8  hash[16]; /**< MD5 hash */
    s8  challege[16];
    u32 challege_len;
    s8  message[16];
    u32 message_len;

    u32 key_status;
    u32 domain_id;
    u32 key_id;
    u32 transport_key;
    u32 storage_key;
    s8  *ca_pub_cert;
    s8  *server_pub_cert;
    s8  *client_pub_cert; 
    s8  *client_pri_cert; 
} __attribute__((__packed__));

#define max_conn_iov_slot   8
#define max_conn_cmd_num    64

enum io_state {
    io_init             = 0,
    io_read_half        = 1,
    io_read_done        = 2,
    io_write_half       = 3,
    io_write_done       = 4,
    io_close            = 5,
} __attribute__((__packed__));

enum rcv_stage {
    stage_recv_bhs      = 0x01,
    stage_recv_data     = 0x02,
    stage_recv_next     = 0x03,
}__attribute__((__packed__));

enum snd_stage {
    stage_send_bhs      = 0x01,
    stage_send_data     = 0x02,
}__attribute__((__packed__));

struct conn_desc {
    struct msghdr rx_msghdr;
    struct msghdr tx_msghdr;

    struct iovec rx_iov[max_conn_iov_slot]; 
    struct iovec tx_iov[max_conn_iov_slot]; 

    enum snd_stage send_stage;
    enum rcv_stage recv_stage;

    enum io_state send_state;
    enum io_state recv_state;

    u32 rx_iosize;
    u32 rx_iovcnt;
    u32 rx_iovused;
    u32 rx_iovoffset;

    u32 tx_iosize;
    u32 tx_iovcnt;
    u32 tx_iovused;
    u32 tx_iovoffset;
} __attribute__((__packed__));

struct conn {
    s32 fd;
    void *curr_recv_cmd;
    struct basic_head bhs;
    struct conn_desc desc;
}__attribute__((__packed__));

enum buffer_idx {
    buff_zero,
    buff_64B,
    buff_128B,
    buff_256B,
    buff_512B,
    buff_1K,
    buff_2K,
    buff_4K,
    buff_8K,
    buff_max,
}__attribute__((__packed__));

struct cmd {
    struct list_head cmd_to_list; /* link to freelist, txlist or acklist */
    struct list_head cmd_to_conn; /* link to conn, close conn*/

    sem_t ack_sem;    /*semaphore used in queue*/

    struct basic_head bhs;
    struct conn *cmd_conn;
    u32     cmd_state;

    u32     buff_idx;
    void    *buffer;
    u32     total_len;  /*the total length of buffer*/
    u32     used_len;   /*used length of buffer*/

    /* https://blog.csdn.net/ubuntu64fan/article/details/17629509 */
    /* ref_cnt aim to solve wildpointer and object, pointer retain */
    u32     ref_cnt;
    srvcb   callback;   /*callback called in thread pool*/
} __attribute__((__packed__));


enum cluster_role {
    cluster_invalid = 0,
    cluster_master  = 1,
    cluster_slaver  = 2,
} __attribute__((__packed__));

struct cluster_node {
    u32 node_id;
    s8  node_name[32];
    u32 node_state;
    u32 node_role;

    s8 node_ipv4[64];
    s8 node_ipv6[64];
} __attribute__((__packed__));


struct client {
    u32     flags;
    u32     state;
    s8      name[32];
    u32     cid;
    srvcb   req_scb;
    srvcb   ack_scb;

    s8      server_host[32];
    s8      server_addr[32];
    s8      server_port[32];

    u32     chap;
    //u32   auth_state;
    u32     auth_method;
    union {
        struct {
            u32 digest_alg;
            u32 id;
            u32 challenge_size;
            u8 *challenge;
        } chap;
    } auth;

    //auth_param	auth;
    //enum auth_state	state; /**< Access state */
    //enum auth_stage	stage;

    struct conn ev_conn;
    struct conn cli_conn;

    s32     timer_fd;
    s32     signal_fd;

    struct list_head tx_cmd_list;
    pthread_spinlock_t tx_cmd_lock;

    struct list_head ack_cmd_list;
    pthread_spinlock_t ack_cmd_lock;
    
    pthread_t rx_tid;
    pthread_t tx_tid;
    
    struct msf_event_base *ev_rx_base;
    struct msf_event_base *ev_tx_base;
    struct msf_event *ev_rx;
    struct msf_event *ev_tx;

    pthread_spinlock_t free_cmd_lock[buff_max];
    struct list_head free_cmd_list[buff_max];

    struct timeval lasttime;
} __attribute__((__packed__));

s32 client_init(s8 *name, s8 *host, s8 *port, srvcb req_scb, srvcb ack_scb);
s32 client_deinit(void);
s32 client_service(struct basic_pdu *pdu);

