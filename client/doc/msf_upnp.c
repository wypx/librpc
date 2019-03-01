
#include "client.h"

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

    printf("UPnP req service callback data(%p) len(%d) cmd(%d).\n", data, len, cmd);
    
    if (UPNP_GET_PARAM == cmd) {
        if (data && len == sizeof(struct upnp_param_t)) 
            memcpy(data, &upnp, sizeof(upnp));
        else
            printf("Callback param is error.\n");
    } else if (RPC_DEBUG_ON == cmd) {

    } else if (RPC_DEBUG_OFF == cmd) {

    }
    return 0;
}

s32 upnp_ack_scb(s8 *data, u32 len, u32 cmd) {

    printf("UPnP ack service callback data(%p) len(%d) cmd(%d).\n",
                   data, len, cmd);

    return 0;
}

#define server_host "192.168.58.132"
#define server_port "9999"

s32 main () {

    s32 rc = -1;

    #define UPNP_NAME "rpc_upnp"
    memcpy(upnp.friend_name, UPNP_NAME, strlen(UPNP_NAME));
    upnp.upnp_nat = 1;
    upnp.upnp_discovery = 2;

    rc = client_init("upnp", local_host_v4, server_port, upnp_req_scb, upnp_ack_scb);
    if (rc < 0) return -1;
    
    while (1) 
      sleep(2);

    client_deinit();

    return 0;
}

