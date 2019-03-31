
#include "client.h"

#include <msf_mempool.h>

#define MSF_MOD_DLNA "DLNA"
#define MSF_DLNA_LOG(level, ...) \
    log_write(level, MSF_MOD_DLNA, __func__, __FILE__, __LINE__, __VA_ARGS__)

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


s32 dlna_req_scb(s8 *data, u32 len, u32 cmd) {

    MSF_DLNA_LOG(DBG_INFO, "DLNA req service callback data(%p) len(%d) cmd(%d).",
                data, len, cmd);

    if (DLNA_GET_PARAM == cmd) {
        memcpy(data, &upnp, sizeof(upnp));
    }
    return 0;
}

struct upnp_param_t upnp;

s32 dlna_ack_scb(s8 *data, u32 len, u32 cmd) {

    MSF_DLNA_LOG(DBG_INFO, "DLNA ack service callback data(%p) len(%d) cmd(%d).",
                   data, len, cmd);

    if (UPNP_GET_PARAM == cmd) {
        msf_memzero(&upnp, sizeof(upnp));
        memcpy(&upnp, data, sizeof(upnp));
        MSF_DLNA_LOG(DBG_DEBUG, "UPnP friend name(%s).", upnp.friend_name);
        MSF_DLNA_LOG(DBG_DEBUG, "UPnP nat enable(%d).", upnp.upnp_nat);
        MSF_DLNA_LOG(DBG_DEBUG, "UPnP discovery enable(%d).", upnp.upnp_discovery);
    }

    return 0;
}

#define server_host "192.168.58.132"
#define server_port "9999"

s32 main () {

    s32 rc = -1;
    s32 reg_mr = true;
    
    struct msf_mempool *mempool = msf_mempool_create_prv(RPC_UPNP_ID, (reg_mr ? MSF_MEMPOOL_FLAG_REG_MR : 0) |
            MSF_MEMPOOL_FLAG_HUGE_PAGES_ALLOC);
    
    msf_mempool_dump(mempool);

    struct msf_reg_mem reg_mem;

    //msf_mem_alloc();

    msf_mempool_alloc(mempool, 8096, &reg_mem);
    msf_mempool_dump(mempool);
    msf_mempool_alloc(mempool, 8096, &reg_mem);
    msf_mempool_dump(mempool);
    msf_mempool_alloc(mempool, 8096, &reg_mem);

    msf_mempool_dump(mempool);

    msf_mempool_free(&reg_mem);

    msf_mempool_dump(mempool);

    msf_mempool_destroy(mempool);
    
    rc = client_init(MSF_MOD_DLNA, local_host_v4, server_port, dlna_req_scb, dlna_ack_scb);
    if (rc < 0) return -1;

    memset(&upnp, 0, sizeof(struct upnp_param_t));

    struct basic_pdu pdu;
    msf_memzero(&pdu, sizeof(struct basic_pdu));

    pdu.dstid = RPC_UPNP_ID;
    pdu.cmd = UPNP_GET_PARAM;
    pdu.payload = NULL;
    pdu.paylen = 0;
    pdu.timeout = 5000;
    pdu.restload = (s8*)&upnp;
    pdu.restlen = sizeof(struct upnp_param_t);

    rc = client_service(&pdu);

    if (rc != RPC_EXEC_SUCC) {
        MSF_DLNA_LOG(DBG_ERROR, "UPnP rpc call service errcode is %d.", rc);
    } else {
        MSF_DLNA_LOG(DBG_DEBUG, "UPnP friend name(%s).", upnp.friend_name);
        MSF_DLNA_LOG(DBG_DEBUG, "UPnP nat enable(%d).", upnp.upnp_nat);
        MSF_DLNA_LOG(DBG_DEBUG, "UPnP discovery enable(%d).", upnp.upnp_discovery);
    }

    pdu.timeout = MSF_NO_WAIT;
    pdu.restload = NULL;
    pdu.restlen = sizeof(struct upnp_param_t);

    while (1) {
        
        rc = client_service(&pdu);
        if (rc != RPC_EXEC_SUCC) {
           MSF_DLNA_LOG(DBG_ERROR, "UPnP rpc call service errcode is %d.", rc);
        } 
        sleep(2);
    }

    client_deinit();

    return 0;
}


