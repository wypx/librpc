/**************************************************************************
*
* Copyright (c) 2018, luotang.me <wypx520@gmail.com>, China.
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

s32 main(s32 argc, s8 *argv[]) {

    if (unlikely((3 != argc && 2 != argc) 
        || (3 == argc && !argv[2]))) {
      //return -1;
    }

    s32 rc = -1;
    s8 buf[PATH_MAX] = { 0 };

    rc = readlink("/proc/self/exe", buf, PATH_MAX-1);
    if (rc < 0 || rc >= PATH_MAX-1) {
       return -1;
    }

    MSF_AGENT_LOG(DBG_DEBUG, "Msf shell excute path: %s.", buf);

    if (server_init() < 0) return -1;

    for ( ;; ) {
        sleep(1);
    }
    return 0;
}

