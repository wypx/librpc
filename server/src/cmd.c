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

s32 cmd_init(void) {

    u32 cmd_idx;
    struct cmd *tmp_cmd = NULL;

    if (unlikely(0 == srv->max_cmd)) {
        MSF_AGENT_LOG(DBG_ERROR, "Config max cmd num is zero.");
        return -1;
    }

    srv->free_cmd = MSF_NEW(struct cmd, srv->max_cmd);
    if (!srv->free_cmd) {
        MSF_AGENT_LOG(DBG_ERROR, "Failed to alloc cmds, max_cmd = %d.", srv->max_cmd);
        return -1;
    }

    pthread_spin_init(&srv->cmd_lock, 0);

    INIT_LIST_HEAD(&srv->free_cmd_list);
    
    for (cmd_idx = 0; cmd_idx < srv->max_cmd; cmd_idx++) {
        tmp_cmd = srv->free_cmd + cmd_idx;
        INIT_LIST_HEAD(&tmp_cmd->cmd_to_list);
        list_add_tail(&tmp_cmd->cmd_to_list, &srv->free_cmd_list);
    }
return 0;
}

struct cmd *cmd_new(void) {

    struct cmd *new_cmd = NULL;

    pthread_spin_lock(&srv->cmd_lock);

    new_cmd = list_first_entry_or_null(&srv->free_cmd_list, 
                struct cmd, cmd_to_list);

    pthread_spin_unlock(&srv->cmd_lock);

    if (!new_cmd) {
        MSF_AGENT_LOG(DBG_ERROR, "Failed to get one cmd.");
        srv->fail_cmds++;
    } else {
        list_del_init(&new_cmd->cmd_to_list);
        msf_memzero(new_cmd->cmd_buff, sizeof(new_cmd->cmd_buff));
        srv->active_cmd++;
    }
    return new_cmd;
}

void cmd_free(struct cmd *old_cmd) {

    pthread_spin_lock(&srv->cmd_lock);
    old_cmd->cmd_conn = NULL;
    list_add_tail(&old_cmd->cmd_to_list, &srv->free_cmd_list);
    pthread_spin_unlock(&srv->cmd_lock);
}

void cmd_deinit(void) {
    pthread_spin_destroy(&srv->cmd_lock);
    sfree(srv->free_cmd);
}

