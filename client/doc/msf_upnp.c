
#include "client.h"

#define MSF_MOD_UPNP "UPNP"
#define MSF_UPNP_LOG(level, ...) \
    msf_log_write(level, MSF_MOD_UPNP, MSF_FUNC_FILE_LINE, __VA_ARGS__)

struct upnp_param_t {
    u8  upnp_nat;
    u8  upnp_discovery;
    u8  reserved[2];
    u8  friend_name[32];
} MSF_PACKED_MEMORY;

struct upnp_param_t upnp;

struct dlna_param_t    {
    u32 enable;
    u32 status;
    s8  confile[64];
    /* set this if you want to customize the name that shows up on your clients */
    s8  friendly_name[32];
} MSF_PACKED_MEMORY;

void upnp_debug_info(struct upnp_param_t *up) {
    MSF_UPNP_LOG(DBG_DEBUG, "UPnP friend name(%s).", up->friend_name);
    MSF_UPNP_LOG(DBG_DEBUG, "UPnP nat enable(%d).", up->upnp_nat);
    MSF_UPNP_LOG(DBG_DEBUG, "UPnP discovery enable(%d).", up->upnp_discovery);
}

s32 upnp_req_scb(s8 *data, u32 len, u32 cmd) {

    MSF_UPNP_LOG(DBG_DEBUG, "UPnP req service callback data(%p) len(%d) cmd(0x%x).", data, len, cmd);

    switch (cmd) {
        case UPNP_GET_PARAM:
            if (data && len == sizeof(struct upnp_param_t)) {
                MSF_UPNP_LOG(DBG_DEBUG, "@@@@UPnP set.");
                memcpy(data, &upnp, sizeof(upnp));
                upnp_debug_info(&upnp);
            }
            break;
        case UPNP_SET_PARAM:
            if (data && len == sizeof(struct upnp_param_t)) {
                MSF_UPNP_LOG(DBG_DEBUG, "@@@@UPnP get.");
                memcpy(&upnp, data, sizeof(upnp));
                upnp_debug_info(&upnp);
            }
            break;
        case RPC_DEBUG_ON:
        case RPC_DEBUG_OFF:
            break;
        default:
            MSF_UPNP_LOG(DBG_ERROR, "Upnp unknown cmd(%u).", cmd);
            break;
    }
    return 0;
}

s32 upnp_ack_scb(s8 *data, u32 len, u32 cmd) {

     MSF_UPNP_LOG(DBG_INFO, "UPnP ack service callback data(%p) len(%d) cmd(%d).",
                   data, len, cmd);

    return 0;
}

#define server_host "192.168.58.132"
#define SERVER_PORT "9999"

s32 main () {

    s32 rc = -1;

    struct client_param param;
    param.name = MSF_MOD_UPNP;
    param.cid = RPC_UPNP_ID;
    param.host = LOCAL_HOST_V4;
    param.port = SERVER_PORT;
    param.req_scb = upnp_req_scb;
    param.ack_scb = upnp_ack_scb;
    
    memcpy(upnp.friend_name, MSF_MOD_UPNP, strlen(MSF_MOD_UPNP));
    upnp.upnp_nat = MSF_TRUE;
    upnp.upnp_discovery = 2;

    rc = client_agent_init(&param);
    if (rc < 0) return -1;
    
    while (1) 
      sleep(2);

    client_agent_deinit();

    return 0;
}

