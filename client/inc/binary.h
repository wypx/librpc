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
#ifndef _BINARY_H_
#define _BINARY_H_

#include <msf_utils.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * rpc_packet define (little endian)
 * [rpc_header][rpc_payload]
 *
 * gipc_header define
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 * |7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           version=32                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           magic=32                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           resid=32                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           desid=32                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           opcode=32                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           paylen=32                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           restlen=32                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           command=32                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           ack=32                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           err=32                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           mseq=32                             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           chsum=32                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           timeout=32                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+ time_stamp=64 +-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *
 * message_id define( resid and desid)
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 * |7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  group_id=7 |unused=5 |R|D|P=2|         cmd_id=16             |
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *  [31~25]: group id
 *         - max support 128 group, can be used to service group
 *  [24~20]: unused
 *  [   19]: return indicator
 *         - 0: no need return
 *         - 1: need return
 *  [   18]: direction
 *         - 0: UP client to server
 *         - 1: DOWN server to client
 *  [17~16]: parser of payload message
 *         - 0 json
 *         - 1 protobuf
 *         - 2 unused
 *         - 3 unused
 *  [15~ 0]: cmd id, defined gipc_command.h
 *         - 0 ~ 7 inner cmd
 *         - 8 ~ 255 user cmd
 *
 * Note: how to add a new bit define, e.g. foo:
 *       1. add foo define in this commet;
 *       2. define IPC_FOO_BIT and IPC_FOO_MASK, and add into BUILD_IPC_MSG_ID;
 *       3. define GET_IPC_FOO;
 *       4. define enum of foo value;
 ******************************************************************************/

#define RPC_VERSION     (0x0100) 
#define RPC_VERINFO     "libipc-1.0"

#define RPC_REQ         0x1
#define RPC_ACK         0x2
#define RPC_CANCEL      0x3
#define RPC_KALIVE      0x4

#define RPC_MAGIC       0x12345678

/* Note: 0x1 - 0x10 is reseved id */
#define RPC_DLNA_ID     0x11
#define RPC_UPNP_ID     0x12
#define RPC_HTTP_ID     0x13
#define RPC_DDNS_ID     0x14
#define RPC_SMTP_ID     0x15
#define RPC_STUN_ID     0x16
#define RPC_MOBILE_ID   0x17
#define RPC_STORAGE_ID  0x18
#define RPC_DATABASE_ID 0x19

/* */
#define RPC_MSG_SRV_ID   0xfffffff1
#define RPC_LOGIN_SRV_ID 0xfffffff2
#define RPC_IMAGE_SRV_ID 0xfffffff3

#define RPC_KEEP_ALIVE_SECS 60
#define RPC_KEEP_ALIVE_TIMEOUT_SECS (7*60+1) /* has 7 tries to send a keep alive */

enum RPC_ERRCODE_ID {
    RPC_EXEC_SUCC       = 0,
    RPC_LOGIN_FAIL      = 1,
    RPC_PEER_OFFLINE    = 2,
};

/* Message types */
enum RPC_COMMAND_ID {
    /* Command for rpc daemon process,
     * such as subscibe messages */
    RPC_LOGIN           = 0x01,
    RPC_LOGOUT          = 0x02,
    RPC_TRANSMIT        = 0x03,
    RPC_DEBUG_ON        = 0x04,
    RPC_DEBUG_OFF       = 0x05,

    /* Command for plugins and compoments */
    MOBILE_SET_PARAM    = 0x10,
    MOBILE_GET_PARAM    = 0x11,

    SMTP_SET_PARAM      = 0x21,
    SMTP_GET_PARAM      = 0x22,
    DDNS_SET_PARAM      = 0x23,
    DDNS_GET_PARAM      = 0x24,
    UPNP_SET_PARAM      = 0x25,
    UPNP_GET_PARAM      = 0x26,
    DLNA_SET_PARAM      = 0x27,
    DLNA_GET_PARAM      = 0x28,
    STUN_SET_PARAM      = 0x29,
    STUN_GET_PARAM      = 0x2a,
};

enum pack_type {
    packet_binary,
    packet_json,
};

struct basic_head {
    u32 version;/* high 8 major ver, low 8 bug and func update */
    u32 magic;  /* Assic:U:0x55 I:0x49 P:0x50 C:0x43 */

    u32 srcid;
    u32 dstid;
    u32 opcode;
    u32 cmd;
    u32 seq;
    u32 errcode;

    u32 datalen;
    u32 restlen;

    u32 checksum;   /* Message Header checksum */
    u32 timeout;    /* Timeout  wait for data */

    s8  reserved[8];
} __attribute__((__packed__));


struct login_pdu {
    s8  name[32];
    s8  hash[32];/* name and pass do hash */
    u32 chap;
}__attribute__((__packed__));

struct basic_pdu {
    u32 dstid;
    u32 cmd;
    u32 timeout;
    s8  *payload;
    u32 paylen;
    s8  *restload;
    u32 restlen;
} __attribute__((__packed__));

#ifdef __cplusplus
}
#endif
#endif
