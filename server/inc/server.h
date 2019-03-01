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

#ifndef _SERVER_H_
#define _SERVER_H_
#define _GNU_SOURCE

#include <msf_list.h>
#include <msf_network.h>
#include <msf_process.h>
#include <binary.h>
#include <dict.h>

#include <sys/sysinfo.h>
#include <sys/resource.h>

#define max_config_len      128
#define max_conn_number     1024
#define max_conn_name       32
#define max_listen_backlog  8
#define max_epoll_event     256
#define max_unix_path_len   256
#define max_reserverd_len   256

#define max_conn_iov_slot   8
#define max_conn_cmd_len    2048
#define max_conn_cmd_num    64


#define	default_config_path     "/mnt/hgfs/tomato/packet/config/msf_rpc_srv.conf"
#define	default_pid_path        "/mnt/hgfs/tomato/packet/config/msf_rpc_srv.pid"
#define default_unix_path       "/var/msf_rpc_srv.sock"
#define	default_log_path        "/mnt/hgfs/tomato/packet/logger/msf_rpc_srv.log"


/* enum of index available in the rpc_agent.conf */
enum config_idx {
    config_invalid = 0,
    config_pid_file,
    config_svc_config,
    config_daemon,
    config_verbose,
    config_backlog,
    config_node_ip,
    config_unix_enable,
    config_unix_path,
    config_unix_access_mask,
    config_net_enable,
    config_net_protocol,
    config_tcp_port,
    config_udp_port,
    config_auth_chap,
    config_max_conns,
    config_max_threads,
    config_packet_type,
    config_log_level,
};

struct config_option {
    enum config_idx id;
    s8 value[max_config_len];
} __attribute__((__packed__));


enum io_state {
    io_init             = 0,
    io_read_half        = 1,
    io_read_done        = 2,
    io_write_half       = 3,
    io_write_done       = 4,
    io_close            = 5,
} __attribute__((__packed__));

enum opcode {
    /* Client to Server Message Opcode values */
    op_nopout_cmd   = 0x01,
    op_login_cmd    = 0x02,
    op_data_cmd     = 0x03,

	/* Server to Client Message Opcode values */
    op_nopin_cmd    = 0x10,
    op_login_rsp    = 0x11,
    op_data_rsp     = 0x12,
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

struct  network_ops {
	s32 (*s_sock_init)(s8 *data, u32 len);
	s32 (*s_option_cb)(s32 fd);
	s32 (*s_read_cb)(s8 *data, u32 len);
	s32 (*s_write_cb)(s8 *data, u32 len);
	s32 (*s_drain_cb)(s8 *data, u32 len);
	s32 (*s_close_cb)(s8 *data, u32 len);
}__attribute__((__packed__));

struct rx_thread {	  
	pthread_t	tid;        /* unique ID id of this thread */
	s32 	thread_idx;
	s8		*thread_name; 

	s32		epoll_num;
	s32		epoll_fd;
	s32		event_fd;
	s32		timer_fd;
          
} __attribute__((__packed__));

struct tx_thread {
    pthread_t   tid;        /* unique ID id of this thread */
    s32         thread_idx;
    sem_t       *thread_sem;
    s8          *thread_name;

    sem_t       sem;
    pthread_spinlock_t tx_cmd_lock;
    struct list_head tx_cmd_list; /* queue of tx connections to handle*/
} __attribute__((__packed__));


struct cmd {
    struct list_head cmd_to_list; /* link to freelist or txlist*/
    struct list_head cmd_to_conn; /* link to conn, close conn*/

    struct basic_head bhs;
    struct conn *cmd_conn;
    u32 cmd_state;
    s8  cmd_buff[max_conn_cmd_len];
} __attribute__((__packed__));

struct srv_listen {
    s32 fd;
    struct sockaddr *sockaddr; 
    socklen_t socklen;

    s32 backlog; //
    s32 rcvbuf;//内核中对于这个套接字的接收缓冲区大小
    s32 sndbuf;//内核中对于这个套接字的发送缓冲区大小

#if (HAVE_KEEPALIVE_TUNABLE)
    s32 keepidle;
    s32 keepintvl;
    s32 keepcnt;
    u32 keepalive:2;
#endif

    void *servers; 

    u32 ocket_valid;
    u32 inherited:1;   /* inherited from previous process */
    u32 nonblocking_accept:1; 
    u32 nonblocking:1;
    u32 reuseport:1;
    u32 add_reuseport:1;

#if (HAVE_DEFERRED_ACCEPT)
    u32 deferred_accept:1;//SO_ACCEPTFILTER(freebsd所用)设置  TCP_DEFER_ACCEPT(LINUX系统所用)
    u32 delete_deferred:1;

    u32 add_deferred:1; //SO_ACCEPTFILTER(freebsd所用)设置  TCP_DEFER_ACCEPT(LINUX系统所用)
#endif

#if (HAVE_TCP_FASTOPEN)
    s32 fastopen;
#endif
} __attribute__((__packed__));


struct chap_param {
    u32     auth_state;
    u32     auth_method;
    s8      auth_user[32];
    s8      auth_hash[32];
} __attribute__((__packed__));

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
    s8  name[max_conn_name];
    s32 clifd;
    u32 cid;

    u32 state;
    u32 timedout:1;
    u32 close:1; //为1时表示连接关闭
    u32 sendfile:1;
    u32 sndlowat:1;
    u32 tcp_nodelay:2; //域套接字默认是disable
    u32 tcp_nopush:2;

    sds key;

    struct list_head conn_to_free; /* link to tx */

    struct basic_head bhs;
    struct chap_param chap;

    struct conn_desc desc;

    struct list_head snd_cmd_list; /* queue of tx connections to handle*/
    struct list_head rcv_cmd_list; /* queue of rx connections to handle*/

    struct rx_thread *rx;
    struct tx_thread *tx;

    pthread_mutex_t lock;
}__attribute__((__packed__));


struct server {
    struct config_option *config_array;
    s32         config_num;

    pthread_t   tid;
    pid_t       pid;
    s32         daemon;
    s32         verbose;
    s8          *pid_file;  
    s8          *conf_file;
    s8          *svc_file;
    s8          *log_file;

    struct conn *listen_net_v4;
    struct conn *listen_net_v6;
    struct conn *listen_unix;

    s32     backlog;    /* default 8 */

    s32     unix_enable;
    s32     unix_socket;
    s32     access_mask; /* access mask (a la chmod) for unix domain socket */
    s8     *unix_path;

    s32     net_enable_v4;
    s32     net_enable_v6;
    s32     net_socket_v4;
    s32     net_socket_v6;
    s32     net_prot_v4;   /* network protocol */
    s32     net_prot_v6;
    s32     tcp_port;
    s32     udp_port;
    s8      *serv_node_v4; /* Which node(ip) to listen, if NULL is INADDR_ANY */;
    s8      *serv_node_v6;

    s32     pack_type;

    s32     stop_flags;
    s32     status;     /* server running status now */

    u32     max_fds;
    u32     max_bytes;  /* 64*1024*1025 64MB */
    u32     cur_conns;  /* Current connection numbers */
    u32     max_conns;   /* Max online client, different from backlog */  
    struct conn         *conns;
    struct list_head    free_conn_list;
    pthread_spinlock_t  conn_lock;

    dict    *conn_dict;

    /*
     * Each thread instance has a wakeup pipe, which other threads
     * can use to signal that they've put a new connection on its queue.
     */
    s32     max_thread;	/* Create n worker thread */
    s32     max_cores;
    s32     used_cores;

    u32     max_cmd;
    u32     active_cmd;
    u32     fail_cmds;
    struct list_head free_cmd_list;
    struct cmd  *free_cmd;
    pthread_spinlock_t cmd_lock;

    pthread_t   listen_tid;
    s32         listen_num;
    s32         listen_ep_fd;
    struct rx_thread *rx_threads;
    struct tx_thread *tx_threads;

    sem_t       mgt_sem;
    pthread_t   mgt_tid;
    s32         mgt_ep_fd;
    /*
     * Number of worker threads that have finished setting themselves up.
     */
    s32             init_count;
    pthread_mutex_t init_lock;
    pthread_cond_t  init_cond;

    s8              reserved[max_reserverd_len];
}__attribute__((__packed__));


extern struct server *srv;

s32 server_init(void);
void server_deinit(void);

#endif
