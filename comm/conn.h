/**************************************************************************
*
* Copyright (c) 2017-2019, luotang.me <wypx520@gmail.com>, China.
* All rights reserved.
*
* Distributed under the terms of the GNU General Public License v2.
*
* This software is provided 'as is' with no explicit or implied warranties
* in respect of its properties, including, but not limited to, correctness
* and/or fitness for purpose.
*
**************************************************************************/
#include <msf_errno.h>
#include <msf_atomic.h>
#include <msf_process.h>
#include <msf_thread.h>
#include <msf_event.h>
#include <msf_file.h>
#include <msf_sem.h>
#include <protocol.h>
#include <sds.h>
#include <limits.h>             /* IOV_MAX */

#define MAX_CONN_NAME       32
#define MAX_CONN_IOV_SLOT   8
#define MAX_CONN_CMD_NUM    64

#define MSF_SERVER 1
#define MSF_CLIENT 1

typedef s32 (*srvcb)(s8 *data, u32 len, u32 cmd);

enum AUTH_STATE {
    authorized,     /* Client is authorized to access service */
    restricted,     /* Client has limited access to service (bandwidth, ...) */
    refused,        /* Client is always refused to access service (i.e. blacklist) */
};

enum IO_STATE {
    io_init             = 0,
    io_read_half        = 1,
    io_read_done        = 2,
    io_write_half       = 3,
    io_write_done       = 4,
    io_close            = 5,
};

enum RCV_STAGE {
    stage_recv_bhs      = 0x01,
    stage_recv_data     = 0x02,
    stage_recv_next     = 0x03,
};

enum SND_STAGE {
    stage_send_bhs      = 0x01,
    stage_send_data     = 0x02,
};

struct conn_desc {
    struct msghdr rx_msghdr;
    struct msghdr tx_msghdr;

    struct iovec rx_iov[MAX_CONN_IOV_SLOT]; 
    struct iovec tx_iov[MAX_CONN_IOV_SLOT]; 

    enum SND_STAGE send_stage;
    enum RCV_STAGE recv_stage;

    enum IO_STATE send_state;
    enum IO_STATE recv_state;

    u32 rx_iosize;
    u32 rx_iovcnt;
    u32 rx_iovused;
    u32 rx_iovoffset;

    u32 tx_iosize;
    u32 tx_iovcnt;
    u32 tx_iovused;
    u32 tx_iovoffset;
} MSF_PACKED_MEMORY;

struct chap_param {
    u32     enable;
    u32     state;
    u32     alg;
    s8      user[MAX_CONN_NAME];
    s8      hash[32];
};

struct rx_thread {
    pthread_t   tid;
    s32     thread_idx;/* unique ID id of this thread */
    s8      *thread_name; 

    s32     epoll_num;
    s32     epoll_fd;
    s32     event_fd;
    s32     timer_fd;
};

struct tx_thread {
    pthread_t   tid;        /* unique ID id of this thread */
    s32         thread_idx;
    sem_t       *thread_sem;
    s8          *thread_name;

    sem_t       sem;
    pthread_spinlock_t tx_cmd_lock;
    struct list_head tx_cmd_list; /* queue of tx connections to handle*/
};

struct conn_sockopt {
    u32 timedout:1;
    u32 close:1; /*conn closed state*/
    u32 sendfile:1;
    u32 sndlowat:1;
    u32 tcp_nodelay:2; /* Unix socket default disable */
    u32 tcp_nopush:2;
};

struct conn {
    s32 fd;
    u32 cid;
    u32 state;
    s8  name[MAX_CONN_NAME];
    void *curr_recv_cmd;
    struct basic_head bhs;
    struct conn_desc desc;
    struct chap_param chap;

    struct list_head conn_to_free; /* link to tx */
    struct list_head tx_cmd_list; /* queue of tx connections to handle*/
    struct list_head rx_cmd_list; /* queue of rx connections to handle*/
    pthread_spinlock_t tx_cmd_lock;

    /**/
    sds key;    /*key is used to find conn by cid*/
    struct conn_sockopt net_opt;

    struct rx_thread *rx;
    struct tx_thread *tx;

    pthread_mutex_t lock;

    struct list_head ack_cmd_list;
    pthread_spinlock_t ack_cmd_lock;

    /*for client use*/
    msf_atomic_t msg_seq; /*ack seq num*/
};

struct cmd {
    u32 cmd_state;
    struct list_head cmd_to_list; /* link to freelist, txlist or acklist */
    struct list_head cmd_to_conn; /* link to conn, close conn*/

    sem_t ack_sem;    /*semaphore used in queue*/

    struct basic_head bhs;
    struct conn *cmd_conn;

    srvcb   callback;   /*callback called in thread pool*/
    void    *buffer;
     /* https://blog.csdn.net/ubuntu64fan/article/details/17629509 */
    /* ref_cnt aim to solve wildpointer and object, pointer retain */
    u32     ref_cnt;
    u32     used_len;   /*used length of buffer*/

    /*Don't to memset vars below */
    u32     total_len;  /*the total length of buffer*/
    u32     buff_idx;

    s8      cmd_buff[MAX_CONN_CMD_NUM];
};


