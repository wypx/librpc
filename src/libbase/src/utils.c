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

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>



#define GIPC_NO_WAIT 		0
#define GIPC_WAIT_FOREVER	-1


void swap(int a, int b)  
{  
    a = a ^ b;  
    b = a ^ b;  
    a = a ^ b;  
}  


void nsleep(int ns) {
	struct timespec req;
	req.tv_sec = 0;
	req.tv_nsec = ns;
	nanosleep(&req, 0);
}

void susleep(int s, int us) {
	struct timeval tv;
	tv.tv_sec = s;
	tv.tv_usec = us;
	select(0, NULL, NULL, NULL, &tv);
}

int signal_handler(int sig, sighandler_t handler) {

	fprintf(stderr, "Signal handled: %s.\n", strsignal(sig));
	struct sigaction action;
	action.sa_handler = handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(sig, &action, NULL);

	return 0;
}

#if defined (__linux__)
#include <sys/prctl.h>
#elif defined (WIN32)
  #ifndef localtime_r
    struct tm *localtime_r(time_t *_clock, struct tm *_result)
    {
        struct tm *p = localtime(_clock);
        if (p)
            *(_result) = *p;
        return p;
    }
  #endif
#endif


int daemonize(int nochdir, int noclose)
{
    int fd = -1;

    switch (fork()) {
    case -1:
        return (-1);
    case 0:
        break;
    default:
        _exit(EXIT_SUCCESS);
    }
	/* set files mask */
	umask(0);

    if (setsid() == -1)
        return (-1);

    if (nochdir == 0) {
        if(chdir("/") != 0) {
            perror("chdir");
            exit(EXIT_FAILURE);
        }
    }

	//fclose(stderr);
	//fclose(stdout);
    if (noclose == 0 && (fd = open("/dev/null", O_RDWR, 0)) != -1) {
        if(dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2 stdin");
            return (-1);
        }
        if(dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2 stdout");
            return (-1);
        }
        if(dup2(fd, STDERR_FILENO) < 0) {
            perror("dup2 stderr");
            return (-1);
        }

        if (fd > STDERR_FILENO) {
            if(close(fd) < 0) {
                perror("close");
                return (-1);
            }
        }
    }
    return (0);
}

int sem_wait_i(sem_t* psem, int mswait) {
	int rv = 0;

	//errno==EINTR屏蔽其他信号事件引起的等待中断
	if(mswait == GIPC_NO_WAIT) {
		while((rv = sem_trywait(psem)) != 0  && 
		 	errno == EINTR) {
		 		usleep(40000);
		}
	} else if(mswait == GIPC_WAIT_FOREVER) {                        
         while((rv = sem_wait(psem)) != 0  && 
		 	errno == EINTR) {
		 		usleep(40000);
		}    	           
    } else {                                            
        struct timespec ts;                         
        clock_gettime(CLOCK_REALTIME, &ts );    //获取当前时间
        ts.tv_sec 	+= (mswait / 1000 );        //加上等待时间的秒数
        ts.tv_nsec 	+= ( mswait % 1000 ) * 1000; //加上等待时间纳秒数                         
        while((rv = sem_timedwait(psem, &ts)) != 0 && 
			errno == EINTR) {
		 		usleep(40000);
		}    
    } 
    return rv;                                              
} 


/*
 * Debugging function to print a hexdump of data with ascii, for example:
 * 00000000  74 68 69 73 20 69 73 20  61 20 74 65 73 74 20 6d  this is  a test m
 * 00000010  65 73 73 61 67 65 2e 20  62 6c 61 68 2e 00        essage.  blah..
 */
int print_hexdump(char *data, int len)
{
    int line;
    int max_lines = (len / 16) + (len % 16 == 0 ? 0 : 1);
    int i;
    
    for(line = 0; line < max_lines; line++)
    {
        printf("%08x  ", line * 16);

        /* print hex */
        for(i = line * 16; i < (8 + (line * 16)); i++)
        {
            if(i < len)
                printf("%02x ", (unsigned char)data[i]);
            else
                printf("   ");
        }
        printf(" ");
        for(i = (line * 16) + 8; i < (16 + (line * 16)); i++)
        {
            if(i < len)
                printf("%02x ", (unsigned char)data[i]);
            else
                printf("   ");
        }

        printf(" ");
        
        /* print ascii */
        for(i = line * 16; i < (8 + (line * 16)); i++)
        {
            if(i < len)
            {
                if(32 <= data[i] && data[i] <= 126)
                    printf("%c", data[i]);
                else
                    printf(".");
            }
            else
                printf(" ");
        }
        printf(" ");
        for(i = (line * 16) + 8; i < (16 + (line * 16)); i++)
        {
            if(i < len)
            {
                if(32 <= data[i] && data[i] <= 126)
                    printf("%c", data[i]);
                else
                    printf(".");
            }
            else
                printf(" ");
        }

        printf("\n");
    }

	return 0;
}

/*
 * This hash generation function is taken originally from Redis source code:
 *
 *  https://github.com/antirez/redis/blob/unstable/src/dict.c#L109
 *
 * ----
 * MurmurHash2, by Austin Appleby
 * Note - This code makes a few assumptions about how your machine behaves -
 * 1. We can read a 4-byte value from any address without crashing
 * 2. sizeof(int) == 4
 *
 * And it has a few limitations -
 *
 * 1. It will not work incrementally.
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
 */
unsigned int utils_gen_hash(const void *key, int len)
{
    /* 'm' and 'r' are mixing constants generated offline.
       They're not really 'magic', they just happen to work well.  */
    int seed = 5381;
    const unsigned int m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
     unsigned int h = seed ^ len;

    /* Mix 4 bytes at a time into the hash */
    const unsigned char *data = (const unsigned char *)key;

    while(len >= 4) {
         unsigned int k = *( unsigned int*) data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch(len) {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0]; h *= m;
    };

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return (unsigned int) h;
}

#if 0
/* returns 1 if the given ip address is a loopback address
 * 0 otherwise */
inline static int ip_addr_loopback(struct ip_addr* ip)
{
	if (ip->af== AF_INET)
		return ip->u.addr32[0]==htonl(INADDR_LOOPBACK);
	else if (ip->af==AF_INET6)
		return IN6_IS_ADDR_LOOPBACK((struct in6_addr*)ip->u.addr32);
	return 0;
}
#endif
