/**************************************************************************
*
* Copyright (c) 2017, luotang.me <wypx520@gmail.com>, China.
* All rights reserved.
*
* Distributed under the terms of the GNU General Public License v2.
*
* This software is provided 'as is' with no explicit or implied warranties
* in respect of its properties, including, but not limited to, correctness
* and/or fitness for purpose.
*
**************************************************************************/

#ifndef __GIPC_MESSAGE_H__
#define __GIPC_MESSAGE_H__


#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * gipc_packet define (little endian)
 * [gipc_header][gipc_payload]
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

#define GIPC_NO_WAIT 		0
#define GIPC_WAIT_FOREVER	-1


#define GIPC_REQUEST	 0
#define GIPC_ACK		 1
#define GIPC_CANCEL	 	 2

#define GIPC_MAGIC 		0x55495043 		
				

typedef struct gipc_header_t gipc_header;
typedef struct gipc_packet_t gipc_packet;

struct gipc_header_t {
	unsigned int	version;/* Version : high 8 major ver, low 8 bug and func update */
	unsigned int	magic; 	/* Assic:U:0x55 I:0x49 P:0x50 C:0x43  magic = 0x55495043;*/
	unsigned int 	resid;	/* Resource process ID */
    unsigned int 	desid;	/* Destine  process ID */ 

	unsigned int  	opcode;	/* Request Ack Cancel */
	unsigned int 	paylen;
	unsigned int 	restlen;
	unsigned int	command; /* Save and get Param */
	
	unsigned int	ack; 	/* Ack need */
	unsigned int	err;
    long 	 int 	mseq;	/* Meassage sequence */
	unsigned int 	timeout;/* Timeout  wait for data */

	unsigned int 	chsum;
	unsigned int	times_tamp;

} __attribute__((__packed__));

struct gipc_packet_t {
	unsigned int 			desid;
	unsigned int 			command; 
	unsigned char* 			payload;	/* Payload 	data */
	unsigned int 			paylen;
	unsigned char* 			restload;	/* Result 	data */
	unsigned int 			restlen;
} __attribute__((__packed__));

typedef int (*service_cb)(char* data, int* len, int ctype);

int gipc_init(int id, char* host, char* port, service_cb sercb);
int gipc_deinit(void);
int gipc_call_service(gipc_packet*  pkt, int timeout);

#ifdef __cplusplus
}
#endif
#endif
