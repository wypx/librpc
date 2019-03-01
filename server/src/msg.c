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

#define RPC_REGISTER	"rpc_register"
#define RPC_HEARTBEAT	"rpc_heartbeat"

typedef s32 (*rpc_cb)(void *data, u32 len);

typedef struct {
	s8 	command[32]; 
	rpc_cb 	cb;
} rpc_driver_table;


s32 conn_verify_msg(struct conn *c) {

	return 0;
}


s32 rpc_client_register(void *data, u32 len) {


	return 0;
}

s32 rpc_client_heartbeat(void *data, u32 len) {


	return 0;
}


s32 rpc_client_router(void* data, u32 len) {

	return 0;
}


s32 rpc_server_debug_on(void *data, u32 len) {
	
	
	return 0;
}

s32 rpc_server_debug_off(void *data, u32 len) 
{
	
	
	return 0;
}


static rpc_driver_table _rpc_driver_table[] = {
	{ RPC_REGISTER, 	rpc_client_register	 }, /* rpc_daemon */
	{ RPC_HEARTBEAT,  	rpc_client_heartbeat }, /* rpc_daemon */
	{ "rpc_transmit", 	rpc_client_router	 }, /* rpc_xxxxxx */
	{ "rpc_set_param", 	rpc_client_router	 }, /* rpc_xxxxxx */
	{ "rpc_get_param", 	rpc_client_router	 }, /* rpc_xxxxxx */
	{ "rpc_debug_on", 	rpc_server_debug_on	 }, /* rpc_daemon */
	{ "rpc_debug_off", 	rpc_server_debug_off },	/* rpc_daemon */
};

s32 msg_command_dispatch(struct conn *c)
{
	conn_verify_msg(c);

	return -1;
}


