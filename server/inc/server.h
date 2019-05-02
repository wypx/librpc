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

#define HAVE_GETIFADDRS         1
#define HAVE_TCP_FASTOPEN       1
#define HAVE_DEFERRED_ACCEPT    1
#define HAVE_KEEPALIVE_TUNABLE  1

#define _GNU_SOURCE
#include <msf_cpu.h>
#include <msf_os.h>
#include <msf_svc.h>
#include <conn.h>
#include <dict.h>

#define MSF_MOD_AGENT "AGENT"
#define MSF_AGENT_LOG(level, ...) \
    log_write(level, MSF_MOD_AGENT, MSF_FUNC_FILE_LINE, __VA_ARGS__)

#define max_config_len      128
#define MAX_CONN_NUM        1024
#define max_conn_name       32
#define max_listen_backlog  8
#define max_epoll_event     256
#define max_unix_path_len   256
#define max_reserverd_len   256


#define default_config_path     "/media/psf/tomato/packet/config/msf_agent.conf"
#define default_pid_path        "/media/psf/tomato/packet/config/msf_rpc_srv.pid"
#define default_unix_path       "/var/msf_rpc_srv.sock"
#define default_log_path        "/media/psf/tomato/packet/logger/msf_rpc_srv.log"


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

struct  network_ops {
    s32 (*s_sock_init)(s8 *data, u32 len);
    s32 (*s_option_cb)(s32 fd);
    s32 (*s_read_cb)(s8 *data, u32 len);
    s32 (*s_write_cb)(s8 *data, u32 len);
    s32 (*s_drain_cb)(s8 *data, u32 len);
    s32 (*s_close_cb)(s8 *data, u32 len);
}__attribute__((__packed__));

struct rx_thread {
    pthread_t   tid;
    s32     thread_idx;/* unique ID id of this thread */
    s8      *thread_name; 

    s32     epoll_num;
    s32     epoll_fd;
    s32     event_fd;
    s32     timer_fd;
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


struct srv_listen {
    s32 fd;
    struct sockaddr *sockaddr; 
    socklen_t socklen;

    s32 backlog;
    s32 rcvbuf;
    s32 sndbuf;

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
    s32     net_prot_v4;   /* network protocol: tcp udp muticast*/
    s32     net_prot_v6;
    s32     tcp_port;
    s32     udp_port;
    s8      *serv_node_v4; /* Which node(ip) to listen, if NULL is INADDR_ANY */;
    s8      *serv_node_v6;

    s32     pack_type;

    s32     stop_listen;
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
}MSF_PACKED_MEMORY;

enum mod_idx {
    MOD_OS,
    MOD_CONFIG,
    MOD_SIGNAL,
    MOD_CMD,
    MOD_CONN,
    MOD_THREAD,
    MOD_NETWORK,
    MOD_SERVER,
    MOD_MAX,
} MSF_PACKED_MEMORY;
    
extern struct msf_svc* msf_rpc_module[];

extern struct server *srv;

s32 server_init(void);

#endif
