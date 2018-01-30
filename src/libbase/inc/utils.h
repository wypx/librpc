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

#include <signal.h>
#include <semaphore.h>


int signal_handler(int sig, sighandler_t handler);

int daemonize(int nochdir, int noclose);

int sem_wait_i(sem_t* psem, int mswait);

int print_hexdump(char *data, int len);


void swap(int a, int b) ;



