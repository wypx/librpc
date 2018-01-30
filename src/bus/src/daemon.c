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
#include "common.h"
#include "bus.h"
#include "utils.h"
#include "version.h"
#include "utils.h"

#include "proxy_rt.h"

int main(int argc, char **argv) {

	GBUS_LOG(LV_ERROR, GREEN,"gbus version :0x%x && %s\n", 
			DBUS_VERSION & 0xff00, DBUS_VERINFO);
	
	GBUS_LOG(LV_ERROR, GREEN, 
		"libevent version : %s\n", event_get_version());

	GIPC_PR("gbus_daemon");

	signal_handler(SIGHUP,  SIG_IGN);
	signal_handler(SIGTERM, SIG_IGN);
	signal_handler(SIGPIPE, SIG_IGN);

	/* set stderr non-buffering (for running under, say, daemontools) */
    //setbuf(stderr, NULL);


		/* process arguments */
#ifdef HAVE_GETOPT_LONG
		//while (-1 != (c = getopt_long(argc, argv, shortopts,longopts, &optindex))) 

#else
		//while (-1 != (c = getopt(argc, argv, shortopts))) 
#endif


	//proxy_manager* m = proxy_manger_init();

	SERVER* s = server_init();
	if( !s ) {
		fprintf(stderr, "server_init failed\n");
		goto error;
	} 
#if 0
 	if( server_resource_limit(s) < 0 ) {
		fprintf(stderr, "server_resource_limit failed\n");
		goto error;
	}

	/* daemonize if requested */
    /* if we want to ensure our ability to dump core, don't chdir to / */
    if ( s->daemon ) {
        if (daemonize(s->maxcore, s->verbose) < 0) {
            fprintf(stderr, "failed to daemon() in order to daemonize\n");
            goto error;
        }
    }


	if( server_conn_timeout_check(s) < 0 ) {
		fprintf(stderr, "server_timeout_check failed\n");
	  	goto error;
	}
#endif

	if( server_start(s) < 0 ) {
		fprintf(stderr, "server_start failed\n");
		goto error;
	} 
	
error:	
	server_destroy( s );
	return -1;
}

