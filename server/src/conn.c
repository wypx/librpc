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

/* Functions managing dictionary of callbacks for pub/sub. */
static u64 callbackHash(const void *key) {
    return dictGenHashFunction((const u8*)key, sdslen((const sds)key));
}

static s32 callbackKeyCompare(void *privdata, const void *key1, const void *key2) {
    s32 l1, l2;
    ((void) privdata);

    MSF_AGENT_LOG(DBG_INFO, "Key cmp(%s-%s).", (s8*)key1, (s8*)key2);

    l1 = sdslen((const sds)key1);
    l2 = sdslen((const sds)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

static void callbackKeyDestructor(void *privdata, void *key) {
    ((void) privdata);
    MSF_AGENT_LOG(DBG_INFO, "Key free (%p).", key);
    sdsfree((sds)key);
}

static void callbackValDestructor(void *privdata, void *val) {
    DICT_NOTUSED(privdata);
    MSF_AGENT_LOG(DBG_INFO, "Val free conn(%p).", val);
}

static dictType keyptrDictType = {
    callbackHash,
    NULL,
    NULL,
    callbackKeyCompare,
    callbackKeyDestructor,
    callbackValDestructor
};

s32 conn_init(void) {

    u32 conn_idx;
    struct conn *new_conn = NULL;
    /* We're unlikely to see an FD much higher than maxconns. */
    s32 next_fd = dup(1);
    s32 headroom = 10;      /* account for extra unexpected open FDs */
    struct rlimit rl;

    srv->max_fds = srv->max_conns + headroom + next_fd;

    /* But if possible, get the actual highest FD we can possibly ever see. */
    if (0 == getrlimit(RLIMIT_NOFILE, &rl)) {
        srv->max_fds = rl.rlim_max;
    } else {
        MSF_AGENT_LOG(DBG_ERROR, "Failed to query maximum file descriptor; "
                        "falling back to maxconns.");
    }

    /* next_fd used to count */
    sclose(next_fd);

    if (!(srv->conns = calloc(srv->max_conns, sizeof(struct conn)))) {
        MSF_AGENT_LOG(DBG_ERROR, 
            "Failed to allocate connection structures.");
        return -1;
    }

    pthread_spin_init(&srv->conn_lock, 0);

    INIT_LIST_HEAD(&srv->free_conn_list);

    for (conn_idx = 0; conn_idx < srv->max_conns; conn_idx++) {
        new_conn = &srv->conns[conn_idx];
        INIT_LIST_HEAD(&new_conn->conn_to_free);
        list_add_tail(&new_conn->conn_to_free, &srv->free_conn_list);
    }

    MSF_AGENT_LOG(DBG_INFO, "MSF rpc server \'pid: %d.", getpid());
    srv->conn_dict = dictCreate(&keyptrDictType, NULL);
    if (!srv->conn_dict) {
        MSF_AGENT_LOG(DBG_ERROR, "Failed to create conn dict.");
        return -1;
    }
    return 0;
}


/* Find the conn by id*/
s32 conn_add_dict(struct conn *c) {

    s8 key_id[16] = { 0 };
    
    snprintf(key_id, sizeof(key_id)-1, "key_%u", c->cid);
    c->key = sdsnew(key_id);

    MSF_AGENT_LOG(DBG_INFO, "Add conn(ox%p) key id is (%s).", c, key_id);
    
    return dictAdd(srv->conn_dict, c->key, c);
}

struct conn *conn_find_by_id(u32 cid) {

    struct conn *c = NULL;
    sds key = NULL;
    s8 key_id[16] = { 0 };
    
    snprintf(key_id, sizeof(key_id)-1, "key_%u", cid);
    key = sdsnew(key_id);
    
    c = dictFetchValue(srv->conn_dict, key);

    MSF_AGENT_LOG(DBG_INFO, "Find conn(ox%p) key id is (%s).", c, key_id);

    sdsfree(key);

    return c;
}

void conn_deinit(void) {

    u32 conn_idx;
    struct conn *c;

    for (conn_idx = 0; conn_idx < srv->max_conns; conn_idx++) {
        c = &srv->conns[conn_idx];
        sclose(c->fd);
    }

    pthread_spin_destroy(&srv->conn_lock);
    dictRelease(srv->conn_dict);
    sfree(srv->conns);
}

struct conn *conn_new(s32 new_fd, s16 event) {

    struct conn *new_conn = NULL;

    pthread_spin_lock(&srv->conn_lock);

    new_conn = list_first_entry_or_null(&srv->free_conn_list, 
                struct conn, conn_to_free);

    pthread_spin_unlock(&srv->conn_lock);

    if (new_conn) {

        MSF_AGENT_LOG(DBG_INFO, "Get a new conn(%p) successful.", new_conn);

        msf_socket_debug(new_fd);
        msf_socket_nonblocking(new_fd);

        list_del_init(&new_conn->conn_to_free);
        new_conn->fd = new_fd;
        new_conn->desc.recv_stage  = stage_recv_bhs;
        new_conn->rx = &srv->rx_threads[(new_conn->fd) % (srv->max_thread)];
        new_conn->tx = &srv->tx_threads[(new_conn->fd) % (srv->max_thread)];

        if (unlikely(new_fd == srv->unix_socket
            ||  new_fd == srv->net_socket_v4
            ||  new_fd == srv->net_socket_v6)) {
            msf_add_event(srv->listen_ep_fd, new_fd, event, new_conn);
        } else {
            msf_add_event(new_conn->rx->epoll_fd, new_fd, event, new_conn);
        }
        new_conn->state = true;
    } else {
        MSF_AGENT_LOG(DBG_ERROR, "Fail to get a new conn.");
    }
    return new_conn;
}

void conn_free(struct conn *c) {

    dictDelete(srv->conn_dict, c->key);

    msf_del_event(c->rx->epoll_fd, c->fd);

    sclose(c->fd);

    /* delete cmd in tx cmd_list*/

    pthread_spin_lock(&srv->conn_lock);
    list_add_tail(&c->conn_to_free, &srv->free_conn_list);
    pthread_spin_unlock(&srv->conn_lock);
}

