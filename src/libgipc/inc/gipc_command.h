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

/* Message types */
enum GIPC_COMMAND_ID {
	processes_register,

	miniupnp_set_param,
	miniupnp_get_param,	

	minidlna_set_param,
	minidlna_get_param,

	wireless_set_param,
	wireless_get_param,

	screen_output_open,
	screen_output_close,

	
};

