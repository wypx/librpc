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


enum GIPC_PROCESS_ID {
	GIPC_DEAMON		= 0,
	GIPC_MiniUPNP	= 1,
	GIPC_MiniDNLA	= 2,
	GIPC_WIRELESS	= 3,
	GIPC_WIFI		= 4,
	GIPC_MQTT		= 5,
	GIPC_SMTP		= 6,
	GIPC_DDNS		= 7,
	GIPC_STUN		= 8,
	GIPC_WEBRTC		= 9,
	GIPC_VPN		= 10,
	GIPC_PPPOE		= 11,
	GIPC_RPC		= 12,
	GIPC_LOG		= 13,
	GIPC_MEDIA		= 14,
	GIPC_SQLITE		= 15,
	GIPC_NGINX		= 16,
	GIPC_MAX_PROCESS,
};


typedef struct {
	unsigned int 	process_id;
	unsigned char	process_name[32];
}gipc_process_info;

	
static gipc_process_info GIPC_PLIST[] = {
	
	{ GIPC_DEAMON,		"GIPC_DEAMON"	},
	{ GIPC_MiniUPNP,	"GIPC_MiniUPNP" },
	{ GIPC_MiniDNLA, 	"GIPC_MiniDNLA" },
	{ GIPC_WIRELESS,	"GIPC_WIRELESS"	},
	{ GIPC_WIFI,		"GIPC_WIFI"		},
	{ GIPC_MQTT,		"GIPC_MQTT"		},
	{ GIPC_SMTP,		"GIPC_SMTP"		},
	{ GIPC_DDNS,		"GIPC_DDNS"		},
	{ GIPC_STUN,		"GIPC_STUN"		},
	{ GIPC_WEBRTC,		"GIPC_WEBRTC"	},
	{ GIPC_VPN, 		"GIPC_VPN"		},
	{ GIPC_PPPOE,		"GIPC_PPPOE" 	},
	{ GIPC_RPC, 		"GIPC_RPC"		},
	{ GIPC_LOG, 		"GIPC_LOG"		},
	{ GIPC_MEDIA,		"GIPC_MEDIA" 	},
	{ GIPC_SQLITE,		"GIPC_SQLITE"	},
	{ GIPC_NGINX,		"GIPC_NGINX" 	},
};

static char* PLIST[] = {
	"GIPC_DEAMON",
	"GIPC_MiniUPNP",
	"GIPC_MiniDNLA",
	"GIPC_WIRELESS",
	"GIPC_WIFI",
	"GIPC_MQTT",
	"GIPC_SMTP",
	"GIPC_DDNS",
	"GIPC_STUN",
	"GIPC_WEBRTC",
	"GIPC_VPN",
	"GIPC_PPPOE",
	"GIPC_RPC",
	"GIPC_LOG",
	"GIPC_MEDIA",
	"GIPC_SQLITE",	
	"GIPC_NGINX",
	"GIPC_MAX",
};


