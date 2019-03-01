#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "debug.h"


void sigusr1_handler(int signal)
{
    debug_info("signal %d received\n", signal);
}

pid_t parent_pid = 0;

int main(void)
{
    pid_t pid;
    int status;

    parent_pid = getpid();

    if ((pid = fork()) < 0) 
    {
        return -1;
    } 
    else if (pid > 0) 
    {   /* parent */
        signal(SIGUSR1, sigusr1_handler);
        wait(&status);
    }
    else
    {   /* child */
        kill(parent_pid, SIGUSR1);
    }

    exit(0);
}

