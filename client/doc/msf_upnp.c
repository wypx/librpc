
#include "client.h"

#define MSF_MOD_UPNP "UPNP"
#define MSF_UPNP_LOG(level, ...) \
    log_write(level, MSF_MOD_UPNP, __func__, __FILE__, __LINE__, __VA_ARGS__)

struct upnp_param_t {
    u8  upnp_nat;
    u8  upnp_discovery;
    u8  reserved[2];
    u8  friend_name[32];
}__attribute__((__packed__));

struct upnp_param_t upnp;

struct dlna_param_t    {
    u32 enable;
    u32 status;
    s8  confile[64];
    /* set this if you want to customize the name that shows up on your clients */
    s8  friendly_name[32];
} __attribute__((__packed__));


s32 upnp_req_scb(s8 *data, u32 len, u32 cmd) {

    MSF_UPNP_LOG(DBG_DEBUG, "UPnP req service callback data(%p) len(%d) cmd(0x%x).", data, len, cmd);
    
    if (UPNP_GET_PARAM == cmd) {
        if (data && len == sizeof(struct upnp_param_t)) 
            memcpy(data, &upnp, sizeof(upnp));
        else
             MSF_UPNP_LOG(DBG_ERROR, "Callback param is error.");

        struct upnp_param_t *up = &upnp;
        MSF_UPNP_LOG(DBG_DEBUG, "2UPnP friend name(%s).", up->friend_name);
        MSF_UPNP_LOG(DBG_DEBUG, "2UPnP nat enable(%d).", up->upnp_nat);
        MSF_UPNP_LOG(DBG_DEBUG, "2UPnP discovery enable(%d).", up->upnp_discovery);
    } else if (UPNP_SET_PARAM == cmd) {
        if (data && len == sizeof(struct upnp_param_t)) 
            memcpy(&upnp, data, sizeof(upnp));
        else
            MSF_UPNP_LOG(DBG_ERROR, "Callback param is error.");
        
        struct upnp_param_t *up = data;
        MSF_UPNP_LOG(DBG_DEBUG, "1UPnP friend name(%s).", up->friend_name);
        MSF_UPNP_LOG(DBG_DEBUG, "1UPnP nat enable(%d).", up->upnp_nat);
        MSF_UPNP_LOG(DBG_DEBUG, "1UPnP discovery enable(%d).", up->upnp_discovery);
    } else if (RPC_DEBUG_ON == cmd) {

    } else if (RPC_DEBUG_OFF == cmd) {

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
    
    memcpy(upnp.friend_name, MSF_MOD_UPNP, strlen(MSF_MOD_UPNP));
    upnp.upnp_nat = 1;
    upnp.upnp_discovery = 2;

    rc = client_init(MSF_MOD_UPNP, LOCAL_HOST_V4, SERVER_PORT, upnp_req_scb, upnp_ack_scb);
    if (rc < 0) return -1;
    
    while (1) 
      sleep(2);

    client_deinit();

    return 0;
}

