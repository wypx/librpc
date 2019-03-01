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
#include <client.h>

extern struct client *rpc;

s32 cmd_init(void);
void cmd_deinit(void);
struct cmd *cmd_new(s32 data_len);
void cmd_push_tx(struct cmd *tx_cmd);
void cmd_push_ack(struct cmd *ack_cmd);
struct cmd *cmd_pop_tx(void);
struct cmd *cmd_pop_ack(u32 seq);

#define ITEM_PER_ALLOC 4
#define ITEM_MAX_ALLOC 32

/* every block size in cmd mem pool */
static const u32 g_cmd_len[buff_max] = 
    { 0, 64, 128, 256, 512, 1024, 2048, 4096, 8096 };

static inline s32 get_cmd_idx(u32 datalen) {
    s32 idx;
    for (idx = 0; idx < buff_max; idx++) {
        if (datalen <= g_cmd_len[idx]) {
            return idx;
        }
    }
    return -1;
}

s32 cmd_init(void) {

    u32 buf_idx;

    for (buf_idx = 0; buf_idx < buff_max; buf_idx++) {
        pthread_spin_init(&rpc->free_cmd_lock[buf_idx], 0);
        INIT_LIST_HEAD(&rpc->free_cmd_list[buf_idx]);
    }

    INIT_LIST_HEAD(&rpc->tx_cmd_list);
    INIT_LIST_HEAD(&rpc->ack_cmd_list);

    pthread_spin_init(&rpc->tx_cmd_lock, 0);
    pthread_spin_init(&rpc->ack_cmd_lock, 0);

    return 0;
}

struct cmd *cmd_new_prealloc(u32 idx) {

    s32 i;
    s32 j;
    struct cmd *new_cmd = NULL;
    struct cmd *tmp_cmd = NULL;

    /* Allocate a bunch of items at once to reduce fragmentation */
    new_cmd = MSF_NEW(struct cmd, ITEM_PER_ALLOC);
    if (NULL == new_cmd) {
        pthread_spin_unlock(&rpc->free_cmd_lock[idx]);
        return NULL;
    }

    for (i = 0; i < ITEM_PER_ALLOC; i++) {
        tmp_cmd = &new_cmd[i];
        /* New items only include message header */
        if (0 != g_cmd_len[idx]) {
            tmp_cmd->buffer = MSF_NEW(s8, g_cmd_len[idx]);
            if (NULL == tmp_cmd->buffer) {
                for (j = 0; j < (i+1); j++) {
                    tmp_cmd = &new_cmd[j];
                    sfree(tmp_cmd->buffer);
                }
                sfree(new_cmd);
                pthread_spin_unlock(&rpc->free_cmd_lock[idx]);
                return NULL; 
            }
        }

        INIT_LIST_HEAD(&(tmp_cmd->cmd_to_list));
        INIT_LIST_HEAD(&(tmp_cmd->cmd_to_conn));
        tmp_cmd->buff_idx = idx;
        tmp_cmd->total_len = g_cmd_len[idx];
    }

    /*
     * Link together all the new items except the first one
     * (which we'll return to the caller) for placement on
     * the freelist.
     */
    for (i = 1; i < ITEM_PER_ALLOC; i++) {
        tmp_cmd = &new_cmd[i];
        list_add_tail(&(tmp_cmd->cmd_to_list), &rpc->free_cmd_list[idx]);
    }

    return new_cmd;
}

void cmd_new_default(struct cmd *new_cmd) {

    if (unlikely(!new_cmd)) {
        return;
    }
    
    msf_memzero(new_cmd->buffer, new_cmd->total_len);
    msf_memzero(&new_cmd->bhs, sizeof(struct basic_head));
    refcount_incr(&new_cmd->ref_cnt);
    new_cmd->used_len = 0;
    INIT_LIST_HEAD(&(new_cmd->cmd_to_list));
    INIT_LIST_HEAD(&(new_cmd->cmd_to_conn));
}

/*
 * Returns a fresh connection queue item.
 * pre malloc 64 struct buffer_item*, first get one of them, 
 * if not enough, new malloc one item
 * this way looks like memory pool.
 */
struct cmd *cmd_new(s32 data_len) {

    s32 idx = -1;
    struct cmd *new_cmd = NULL;

    idx = get_cmd_idx(data_len);
    if (idx < 0) return NULL;

    printf("New cmd datalen(%d) idx(%d) toatal len(%u).\n",
        data_len, idx, g_cmd_len[idx]);

    pthread_spin_lock(&rpc->free_cmd_lock[idx]);

    if (list_empty(&rpc->free_cmd_list[idx])) {

        printf("Need to new cmd item.\n");

        new_cmd = cmd_new_prealloc(idx);
        if (unlikely(!new_cmd)) {
            return NULL;
        }
    }else {
        new_cmd = list_first_entry_or_null(&rpc->free_cmd_list[idx], 
                        struct cmd, cmd_to_list);
        if (new_cmd) {
            list_del_init(&(new_cmd->cmd_to_list));
        }
    }
    pthread_spin_unlock(&rpc->free_cmd_lock[idx]);

    cmd_new_default(new_cmd);

    return new_cmd;
}

void cmd_push_ack(struct cmd *ack_cmd) {
    pthread_spin_lock(&rpc->ack_cmd_lock);
    list_add_tail(&ack_cmd->cmd_to_list, &rpc->ack_cmd_list);
    pthread_spin_unlock(&rpc->ack_cmd_lock);
}

void cmd_push_tx(struct cmd *tx_cmd)  {
    pthread_spin_lock(&rpc->tx_cmd_lock);
    list_add_tail(&tx_cmd->cmd_to_list, &rpc->tx_cmd_list);
    pthread_spin_unlock(&rpc->tx_cmd_lock);
}

void cmd_push_tx_head(struct cmd *tx_cmd)  {
    pthread_spin_lock(&rpc->tx_cmd_lock);
    list_add(&tx_cmd->cmd_to_list, &rpc->tx_cmd_list);
    pthread_spin_unlock(&rpc->tx_cmd_lock);
}

/*
 * Looks for an item on a connection queue, but doesn't block if there isn't
 * one. Returns the item, or NULL if no item is available
 */
struct cmd* cmd_pop_ack(u32 seq) {

    struct cmd *ack_cmd = NULL;
    struct cmd *next_cmd = NULL;
    
    pthread_spin_lock(&rpc->ack_cmd_lock);
    
    if (list_empty(&rpc->ack_cmd_list)) {
        fprintf(stderr, "TX ack buffer list is empty.\n");
        pthread_spin_unlock(&rpc->ack_cmd_lock);
        return NULL;
    }

    /*Find the same seq of cmd*/
    list_for_each_entry_safe(ack_cmd, next_cmd, &rpc->ack_cmd_list, cmd_to_list) 
    {
        if (ack_cmd) {
            if (likely(ack_cmd->bhs.seq == seq)) {
                list_del_init(&ack_cmd->cmd_to_list);
                break;
            } else {
                ack_cmd = NULL;
            }
        }
    }
    pthread_spin_unlock(&rpc->ack_cmd_lock);

    fprintf(stderr, "TX ack buffer size(%d).\n", list_size(&rpc->ack_cmd_list));

    if (!ack_cmd) {
        fprintf(stderr, "Failed to pop one ack buffer_item.\n");
    } else {
        //fprintf(stderr, "Pop one ack buffer_item successful.\n");
    }
    return ack_cmd;
}

struct cmd* cmd_pop_tx(void) {

    struct cmd *tx_cmd = NULL;

    pthread_spin_lock(&rpc->tx_cmd_lock);

    if (list_empty(&rpc->tx_cmd_list)) {
        fprintf(stderr, "TX write buffer list is empty.\n");
        pthread_spin_unlock(&rpc->tx_cmd_lock);
        return NULL;
    }

    tx_cmd = list_first_entry_or_null(&rpc->tx_cmd_list, 
                            struct cmd, cmd_to_list);
    
    pthread_spin_unlock(&rpc->tx_cmd_lock);

    if (!tx_cmd) {
        fprintf(stderr, "Failed to get one write buffer_item.\n");
    } else {
        list_del_init(&tx_cmd->cmd_to_list);
    }
    return tx_cmd;
}

void cmd_free(struct cmd *old_cmd) {

    if (!old_cmd) return;

    s32 idx = -1;
    s32 cmd_num = -1;

    idx = old_cmd->buff_idx;

    s32 rc = refcount_decr(&old_cmd->ref_cnt);
    if (rc > 0) return;

    printf("Cmd free total len(%d) idx(%d)(%d).\n", old_cmd->total_len, idx, g_cmd_len[idx]);

    pthread_spin_lock(&rpc->free_cmd_lock[idx]);

    cmd_num = list_size(&rpc->free_cmd_list[idx]);
    
    if (cmd_num > ITEM_MAX_ALLOC) {
        printf("Buffer free item, cur_num(%d), max_num(%d), free it.\n", cmd_num, ITEM_MAX_ALLOC);
        sfree(old_cmd->buffer);
        sfree(old_cmd);
        pthread_spin_unlock(&rpc->free_cmd_lock[idx]);
        return;
    }

    list_add_tail(&old_cmd->cmd_to_list, &rpc->free_cmd_list[idx]);

    pthread_spin_unlock(&rpc->free_cmd_lock[idx]);

    return;
}

void cmd_deinit(void) {

    s32 buf_idx;
    struct cmd *del_cmd = NULL;
    struct cmd *next_cmd = NULL;
    
    for (buf_idx = 0; buf_idx < buff_max; buf_idx++) {
        pthread_spin_destroy(&rpc->free_cmd_lock[buf_idx]);
        if (list_empty(&rpc->free_cmd_list[buf_idx])) {
            continue;
        } else {
            list_for_each_entry_safe(del_cmd, next_cmd, &rpc->free_cmd_list[buf_idx], cmd_to_list) {
                if (del_cmd) {
                    list_del_init(&del_cmd->cmd_to_list);
                    sfree(del_cmd->buffer);
                    sfree(del_cmd);
                }
            }
        }
    }
}

