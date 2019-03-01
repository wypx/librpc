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

s32 conn_init(void);
void conn_deinit(void);
struct conn *conn_new(s32 new_fd, s16 event);
void conn_free(struct conn* c);

/* Functions managing dictionary of callbacks for pub/sub. */
static u64 callbackHash(const void *key) {
    return dictGenHashFunction((const u8*)key, sdslen((const sds)key));
}

static s32 callbackKeyCompare(void *privdata, const void *key1, const void *key2) {
    s32 l1, l2;
    ((void) privdata);

    fprintf(stderr, "Key cmp(%s-%s).\n", (s8*)key1, (s8*)key2);

    l1 = sdslen((const sds)key1);
    l2 = sdslen((const sds)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

static void callbackKeyDestructor(void *privdata, void *key) {
    ((void) privdata);
    fprintf(stderr, "Key free (%p).\n", key);
    sdsfree((sds)key);
}

static void callbackValDestructor(void *privdata, void *val) {
    DICT_NOTUSED(privdata);
    fprintf(stderr, "Val free conn(%p).\n", val);
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
        fprintf(stderr, "Failed to query maximum file descriptor; "
                        "falling back to maxconns.\n");
    }

    /* next_fd used to count */
    close(next_fd);

    if (!(srv->conns = calloc(srv->max_conns, sizeof(struct conn)))) {
        fprintf(stderr, "Failed to allocate connection structures.\n");
        return -1;
    }

    pthread_spin_init(&srv->conn_lock, 0);

    INIT_LIST_HEAD(&srv->free_conn_list);

    for (conn_idx = 0; conn_idx < srv->max_conns; conn_idx++) {
        new_conn = &srv->conns[conn_idx];
        INIT_LIST_HEAD(&new_conn->conn_to_free);
        list_add_tail(&new_conn->conn_to_free, &srv->free_conn_list);
    }

    fprintf(stdout, "MSF rpc server \'pid: %d\r\n", getpid());
    srv->conn_dict = dictCreate(&keyptrDictType, NULL);
    if (!srv->conn_dict) {
        fprintf(stderr, "Failed to create conn dict.\n");
        return -1;
    }
    return 0;
}


/* Find the conn by id*/
s32 conn_add_dict(struct conn *c) {

    s8 key_id[16] = { 0 };
    
    snprintf(key_id, sizeof(key_id)-1, "key_%u", c->cid);
    c->key = sdsnew(key_id);

    printf("Add conn(ox%p) key id is (%s).\n", c, key_id);
    
    return dictAdd(srv->conn_dict, c->key, c);
}

struct conn *conn_find_by_id(u32 cid) {

    struct conn *c = NULL;
    sds key = NULL;
    s8 key_id[16] = { 0 };
    
    snprintf(key_id, sizeof(key_id)-1, "key_%u", cid);
    key = sdsnew(key_id);
    
    c = dictFetchValue(srv->conn_dict, key);

    printf("Find conn(ox%p) key id is (%s).\n", c, key_id);

    sdsfree(key);

    return c;
}

void conn_deinit(void) {
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

        printf("Get a new conn(%p) successful.\n", new_conn);

        socket_debug(new_fd);
        socket_nonblocking(new_fd);

        list_del_init(&new_conn->conn_to_free);
        new_conn->clifd = new_fd;
        new_conn->desc.recv_stage  = stage_recv_bhs;
        new_conn->rx = &srv->rx_threads[(new_conn->clifd) % (srv->max_thread)];
        new_conn->tx = &srv->tx_threads[(new_conn->clifd) % (srv->max_thread)];

        if (unlikely(new_fd == srv->unix_socket
            ||  new_fd == srv->net_socket_v4
            ||  new_fd == srv->net_socket_v4)) {
            msf_add_event(srv->listen_ep_fd, new_fd, event, new_conn);
        } else {
            msf_add_event(new_conn->rx->epoll_fd, new_fd, event, new_conn);
        }
        new_conn->state = true;
    } else {
        printf("Fail to get a new conn.\n");
    }
    return new_conn;
}

void conn_free(struct conn *c) {

    dictDelete(srv->conn_dict, c->key);
    
    msf_del_event(c->rx->epoll_fd, c->clifd);

    sclose(c->clifd);

    /* delete cmd in tx cmd_list*/

    pthread_spin_lock(&srv->conn_lock);
    list_add_tail(&c->conn_to_free, &srv->free_conn_list);
    pthread_spin_unlock(&srv->conn_lock);
}

