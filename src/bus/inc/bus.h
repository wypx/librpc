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
#include "server.h"

#include "glog.h"



#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>


#define GBUS_STR "GBUS"
#define GBUS_LOG(level, color, ...)  glog_write(level, color, GBUS_STR, ##__VA_ARGS__)

SERVER* server_init(void);
int server_start(SERVER* s);
int server_destroy(SERVER* s);

//广播感兴趣的事件
int server_broadcast(SERVER* s);


int server_conn_timeout_check(SERVER* s);
int server_resource_limit(SERVER* s);


