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

static const struct {
    enum config_idx config_id;
    const s8 *config_name;
} optionids[] = {
    { config_pid_file,          "pid_file" },
    { config_svc_config,        "svc_conf" },
    { config_daemon,            "daemon" },
    { config_verbose,           "verbose" },
    { config_backlog,           "backlog"},
    { config_node_ip,           "node_ip"},
    { config_unix_enable,       "unix_enable"},
    { config_unix_path,         "unix_path"},
    { config_unix_access_mask,  "unix_mask"},
    { config_net_enable,        "net_enbale"},
    { config_net_protocol,      "net_protocol"},
    { config_tcp_port,          "tcp_port" },
    { config_udp_port,          "udp_port" },
    { config_max_conns,         "max_conns" },
    { config_max_threads,       "max_threads" },
    { config_packet_type,       "packet_type"},
    { config_auth_chap,         "auth_chap" },
    { config_log_level,         "log_level" },
};

s32 config_init(void) {

    FILE *hfile = NULL;
    s8 buffer[1024];
    s8 *equals;
    s8 *name;
    s8 *value;
    s8 *t;
    s32 linenum = 0;
    u32 i;
    enum config_idx id;

    if (unlikely(!srv->conf_file|| *(srv->conf_file) == '\0'))
        return -1;

    /* only error if file exists or using -f */
    if (access(srv->conf_file, F_OK) != 0) {
        MSF_AGENT_LOG(DBG_ERROR, "Config file %s not exist.", srv->conf_file);
        return -1;
    }

    memset(buffer, 0, sizeof(buffer));

    MSF_AGENT_LOG(DBG_INFO, "Reading config from file %s.", srv->conf_file);

    if (!(hfile = fopen(srv->conf_file, "r")))
        return -1;

    while (fgets(buffer, sizeof(buffer), hfile)) {
        linenum++;
        t = strchr(buffer, '\n'); 
        if (t) {
            *t = '\0';
            t--;
            while ((t >= buffer) && isspace(*t)) {
                *t = '\0';
                t--;
            }
        }

        /* skip leading whitespaces */
        name = buffer;
        while (isspace(*name))
        	name++;

        /* check for comments or empty lines */
        if(name[0] == '#' || name[0] == '\0') continue;

        if (!(equals = strchr(name, '='))) {
            MSF_AGENT_LOG(DBG_ERROR, "Parsing error file %s line %d : %s.",
                    srv->conf_file, linenum, name);
            continue;
        }

        /* remove ending whitespaces */
        for (t=equals-1; t>name && isspace(*t); t--)
            *t = '\0';

        *equals = '\0';
        value = equals+1;

        /* skip leading whitespaces */
        while (isspace(*value))
            value++;

        id = config_invalid;
        for (i = 0; i < _ARRAY_SIZE(optionids); i++) {

            if (0 == strcmp(name, optionids[i].config_name)) {
                id = optionids[i].config_id;
                MSF_AGENT_LOG(DBG_INFO, "%2d %2d %s %s.", i, optionids[i].config_id, name, optionids[i].config_name); 
                break;
            }
        }

        if (id == config_invalid) {
                MSF_AGENT_LOG(DBG_ERROR, "Parsing error file %s line %d : %s=%s.",
                        srv->conf_file, linenum, name, value);
            } else {
                srv->config_num++;
                t = (s8*)MSF_RENEW(srv->config_array, struct config_option, srv->config_num);
                if (!t) {
                    MSF_AGENT_LOG(DBG_ERROR, "Memory allocation error: %s=%s.", name, value);
                    srv->config_num--;
                    continue;
            }
            else {
                srv->config_array = (struct config_option *)t;
            }

            srv->config_array[srv->config_num-1].id = id;
            memcpy(srv->config_array[srv->config_num-1].value, value, 
            	min((s32)strlen(value), max_config_len));
        }

    }

    fclose(hfile);

    return 0;
}

void config_deinit(void) {
    sfree(srv->config_array);
}


