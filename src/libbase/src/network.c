#include "network.h"


/* Server features: depends on server setup and Linux Kernel version */
#define KERNEL_TCP_FASTOPEN      1
#define KERNEL_SO_REUSEPORT      2
#define KERNEL_TCP_AUTOCORKING   4


/* linux-2.6.38.8/include/linux/compiler.h */  
# define likely(x)  	__builtin_expect(!!(x), 1)  
# define unlikely(x)    __builtin_expect(!!(x), 0)

/* linux-2.6.38.8/include/linux/compiler.h */  
# ifndef likely  
#  define likely(x) (__builtin_constant_p(x) ? !!(x) : __branch_check__(x, 1))  
# endif  
# ifndef unlikely  
#  define unlikely(x)   (__builtin_constant_p(x) ? !!(x) : __branch_check__(x, 0))  
# endif 


static int kernel_features = 1;
static int portreuse = 1;


/*
 * Checks validity of an IP address string based on the version
 * AF_INET6 AF_INET
 */
int isipaddr(char *ip, int af_type)
{
    char addr[sizeof(struct in6_addr)];
    int len = sizeof(addr);
    
#ifdef WIN32
    if(WSAStringToAddress(ip, af_type, NULL, PADDR(addr), &len) == 0)
        return 1;
#else /*~WIN32*/    
    if(inet_pton(af_type, ip, addr) == 1)
        return 1;
#endif /*WIN32*/

    return 0;
}

int socket_ip_str(int fd, char **buf,  int size, unsigned long *len) {
    int res = -1;
    struct sockaddr_storage addr;
    socklen_t s_len = sizeof(addr);

	memset(&addr, 0, sizeof(addr));

    res = getpeername(fd, (struct sockaddr*)&addr, &s_len);
    if (unlikely(res == -1)) {
        printf("[FD %i] Can't get addr for this socket\n", fd);
        return -1;
    }
    errno = 0;

    if(addr.ss_family == AF_INET) {
        if((inet_ntop(AF_INET, &((struct sockaddr_in*)&addr)->sin_addr,
                      *buf, size)) == NULL) {
            printf("gsocket_ip_str: Can't get the IP text form (%i)\n", errno);
            return -1;
        }
    } else if(addr.ss_family == AF_INET6) {
        if((inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&addr)->sin6_addr,
                      *buf, size)) == NULL) {
            printf("gsocket_ip_str: Can't get the IP text form (%i)", errno);
            return -1;
        }
    }

    *len = strlen(*buf);
    return 0;
}


/*
 * Example from:
 * http://www.baus.net/on-tcp_cork
 */
int socket_cork_flag(int fd, int state) {
    printf("Socket, set Cork Flag FD %i to %s", fd, (state ? "ON" : "OFF"));

#if defined (TCP_CORK)
    return setsockopt(fd, SOL_TCP, TCP_CORK, &state, sizeof(state));
#elif defined (TCP_NOPUSH)
    return setsockopt(fd, SOL_SOCKET, TCP_NOPUSH, &state, sizeof(state));
#endif
}

int socket_blocking(int fd) {
#ifdef WIN32
	unsigned long nonblocking = 0;
	return ioctlsocket(fd, FIONBIO, &val);
#else
	int val = fcntl(fd, F_GETFL, 0);
	if (fcntl(fd, F_SETFL, val & ~O_NONBLOCK) == -1) {
		return -1;
	}
#endif
	return 0;
}

int socket_nonblocking(int fd) {
#ifdef _WIN32
	{
		unsigned long nonblocking = 1;
		if (ioctlsocket(fd, FIONBIO, &nonblocking) == SOCKET_ERROR) {
			printf(fd, "fcntl(%d, F_GETFL)", (int)fd);
			return -1;
		}
	}
#else
	{
		int flags;
		if ((flags = fcntl(fd, F_GETFL, NULL)) < 0) {
			printf("fcntl(%d, F_GETFL)", fd);
			return -1;
		}
		if (!(flags & O_NONBLOCK)) {
			if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
				printf("fcntl(%d, F_SETFL)", fd);
				return -1;
			}
		}
		
		fcntl(fd, F_SETFD, FD_CLOEXEC);
	}
#endif
	return 0;
}


//http://blog.csdn.net/yangzhao0001/article/details/48003337
int socket_timeout(int fd, int us) {		

	struct timeval tv;

	tv.tv_sec 	= 0;
	tv.tv_usec	= 200000;	//200ms

	if(setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv) < 0 )) {
		printf("setsocketopt SO_SNDTIMEO errno %d\n", errno);
		//return -1;
	}
#if 0
	if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv) < 0 ) {
		printf("setsocketopt failed errno %d\n", errno);
		return -1;
	}
#endif
	return 0;
}

int socket_reuseaddr(int fd) {
#if defined(SO_REUSEADDR) && !defined(_WIN32)
	int one = 1;
	/* REUSEADDR on Unix means, "don't hang on to this address after the
	 * listener is closed."  On Windows, though, it means "don't keep other
	 * processes from binding to this address while we're using it. */
	return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*) &one,
	    (int)sizeof(one));
#else
	return 0;
#endif
}

int socket_reuseport(int fd) {
    int on = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, 
		(int)sizeof(on));
}

int socket_linger(int fd) {
  	struct linger l;
	l.l_onoff = 1;
	l.l_linger = 0;
	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, 
				(void*)&l, sizeof(l)) < 0) {
		printf("setsockopt(SO_LINGER)\n");
		return -1;
	}
    return 0;
}

int socket_alive(int fd) {
	int flags = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, 
				(void*)&flags, sizeof(flags))<0) {
		printf("setsockopt(SO_LINGER)\n");
		return -1;
	}
    return 0;
}

int socket_closeonexec(int fd)
{
#if !defined(_WIN32) 
	int flags;
	if ((flags = fcntl(fd, F_GETFD, NULL)) < 0) {
		return -1;
	}
	if (!(flags & FD_CLOEXEC)) {
		if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
			return -1;
		}
	}
#endif
	return 0;
}


int socket_tcp_nodelay(int fd) {
    int on = 1;
    return setsockopt(fd, SOL_TCP, TCP_NODELAY, &on, sizeof(on));
}

/*
 * Enable the TCP_FASTOPEN feature for server side implemented in
 * Linux Kernel >= 3.7, for more details read here:
 *
 *  TCP Fast Open: expediting web services: http://lwn.net/Articles/508865/
 */
int socket_tcp_fastopen(int fd) {
#if defined (__linux__)
    int qlen = 5;
    return setsockopt(fd, SOL_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen));
#endif

    (void) fd;
    return -1;
}


int socket_tcp_defer_accept(int fd) {
#if defined (__linux__)
    int timeout = 0;
    return setsockopt(fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &timeout, sizeof(int));
#else
    (void) fd;
    return -1;
#endif
}



int socket_create(int domain, int type, int protocol) {
    int fd = -1;

#ifdef SOCK_CLOEXEC
    fd = socket(domain, type | SOCK_CLOEXEC, protocol);
#else
    fd = socket(domain, type, protocol);
    socket_closeonexec(fd);
#endif

    if (fd == -1) {
        perror("socket");
        return -1;
    }
    return fd;
}

int socket_bind(int fd, const struct sockaddr *addr,
                   socklen_t addrlen, int backlog)
{
    int ret = -1;

    ret = bind(fd, addr, addrlen);
    if( ret < 0) {
         int e = errno;
        switch (e) {
        case 0:
            printf("Could not bind socket\n");
            break;
        case EADDRINUSE:
            printf("Port %d for receiving UDP is in use\n", 8888);
            break;
        case EADDRNOTAVAIL:
            break;
        default:
            printf("Could not bind UDP receive port errno:%d\n", e);
            break;
        }
        return -1;
    }

/*
 * Enable TCP_FASTOPEN by default: if for some reason this call fail,
 * it will not affect the behavior of the server, in order to succeed,
 * Monkey must be running in a Linux system with Kernel >= 3.7 and the
 * tcp_fastopen flag enabled here:
 *
 *     # cat /proc/sys/net/ipv4/tcp_fastopen
 *
 * To enable this feature just do:
 *
 *     # echo 1 > /proc/sys/net/ipv4/tcp_fastopen
 */

    if (kernel_features & KERNEL_TCP_FASTOPEN) {
        ret = socket_tcp_fastopen(fd);
        if (ret == -1) {
            printf("Could not set TCP_FASTOPEN\n");
        }
    }

    ret = listen(fd, backlog);
    if(ret < 0) {
        return -1;
    }

    return 0;
}


/* Network helpers */
int connect_to_unix_socket (const char* cname, const char* sname) {
  	int fd  = -1;
  	int len =  0;
  	struct sockaddr_un addr;

	if( !cname || !sname ) {
		goto err;
	}
  
  	fd = socket_create(AF_UNIX, SOCK_STREAM, 0);
 	 if (fd < 0) {
      printf ("Failed to open socket: %s\n", strerror (errno));
      goto err;
  	}

  	if (socket_nonblocking(fd) < 0) {
   		printf ("socket_nonblocking: %s", strerror (errno));
		goto err;
 	}
  	if (socket_linger(fd) < 0) {
   		printf ("socket_linger: %s", strerror (errno));
		goto err;
  	}
  	if (socket_alive(fd) < 0) {
		printf ("socket_alive: %s\n", strerror (errno));
		goto err;
  	}  
	
  	memset (&addr, 0, sizeof(addr));
  	addr.sun_family = AF_UNIX;

  	snprintf(addr.sun_path, sizeof(addr.sun_path) -1, 
  			"%s%s", PREFIX_UNIX_PATH, cname);

  	len = OFF_SETOF(struct sockaddr_un, sun_path) + strlen(addr.sun_path);
  	unlink(addr.sun_path);    /* in case it already exists */

  	if(bind(fd, (struct sockaddr *)&addr, len) < 0) {
     	printf("bind call failed  errno %d fd %d \n", errno, fd);
     	goto err;
  	}
  	/* 暂不设置权限使用默认的0777 CLI_PERM */
  	if (chmod(addr.sun_path, 0777) < 0) {
  		printf("chmod sun_path %s failed \n", addr.sun_path);
  		goto err;
  	}

  	memset(addr.sun_path, 0, sizeof(addr.sun_path));
  	snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, 
			  "%s%s", PREFIX_UNIX_PATH, sname);
  
  	if (connect (fd, (struct sockaddr *) &addr, len) < 0) {
  	 	if (errno == EINPROGRESS) 
            return fd;
        else {
            printf ("[%d] Failed to connect to %s: %s\n", 
					fd, addr.sun_path, strerror (errno));
		    goto err;
        }	
	}
 
  	return fd;
err:
	sclose(fd);
	return -1;
}

int connect_to_host (const char *host, const char *port)
{
	int flag = 1;
	int fd  = -1;
	int ret = -1;
	struct addrinfo hints = { 0, };
	struct addrinfo *res, *r;

	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;


  	ret = getaddrinfo (host, port, &hints, &res);
  	if (ret < 0) {
     	printf ("Connect to %s port %s failed (getaddrinfo: %s)\n",
        		host, port, strerror(errno));
      return -1;
    }

  	for (r = res; r != NULL ; r = r->ai_next) {
      fd = socket (r->ai_family, r->ai_socktype, r->ai_protocol);
      if (fd == -1) {
          continue;
  	  }
#if 1	 
	  ret = connect (fd, r->ai_addr, r->ai_addrlen);
      if (ret == -1) {
	    if (errno == EINPROGRESS) {
			break;
		}
		else {
			printf("%s", strerror(errno));
			sclose(fd);
			continue;
		}

  	  }
#else  //server
	ret = bind(sock->fd, paddr, sock->addr_len);
	ret = listen(sock->fd, BACKLOG);
#endif
      break; /* success */
    }

    freeaddrinfo(res);

    if (r == NULL) {
        return -1;
	}
	
	if (socket_tcp_nodelay(fd) < 0) {
    	printf("socket_tcp_nodelay failed\n");
		return -1;
	}

	if (socket_nonblocking(fd) < 0) {
    	printf("socket_reuseport failed\n");
		return -1;
	}
	
	if (socket_linger(fd) < 0) {
   		printf ("socket_linger: %s", strerror (errno));
		return -1;
  	}
  	if (socket_alive(fd) < 0) {
		printf ("socket_alive: %s\n", strerror (errno));
		return -1;
  	}  
#if 0
	if (socket_reuseport(fd) < 0) {
		printf("socket_nonblocking failed\n");
		return -1;
	}
	

	if (socket_reuseaddr(fd) < 0) {
		printf("socket_nonblocking failed\n");
		return -1;
	}

	if(socket_bind(fd, r->ai_addr, r->ai_addrlen, 128) < 0) {
		printf("Cannot listen on %s:%s\n", host, port);
		return -1;
	}
#endif
  	return fd;
 }


int readn(int fd, void* const buf_, int n) {
	 unsigned char *buf = (unsigned char *) buf_;
	 ssize_t		readnb;

	 do {
		 while ((readnb = read(fd, buf, n)) < 0 &&
				errno == EINTR);
		 if (readnb < 0) {
			 return readnb;
		 }
		 if (readnb == 0) {
			 break;
		 }
		 n -= readnb;
		 buf += readnb;
	 } while (n > 0);

	 return (int) (buf - (unsigned char *) buf_);
}

int recvn(int fd, void* const buf_, int n)
{
	 unsigned char *buf = (unsigned char *) buf_;
	 ssize_t		readnb;

	 do {
		if((readnb = recv(fd, buf, n, 0)) < 0 ) {
			if(errno == EINTR) {
				usleep(200);
				continue;
			} else if (errno == ECONNRESET) {
				printf("Client was reset, fd = [%d]\n", fd);
				return -2;
            }  else {
               printf("read error < 0, errno = %d fd = [%d]\n", errno, fd);
			   return -2;
        	}
	   	} else if (readnb == 0) {
        	printf("Client close fd break, fd = [%d]\n", fd);
			return -3;
        }	
		n -= readnb;
		buf += readnb;
	 } while (n > 0);

	 return (int) (buf - (unsigned char *) buf_);

#if 0
	bytes_recv = recvfrom(fd, data, len, 0,
						SOCK_PADDR(from), &SOCK_LEN(from));
#endif
}

int sendn(const int fd, const void * const buf_, 
						size_t count, const int timeout)
 {
	 struct pollfd	pfd;
	 const char    *buf = (const char *) buf_;
	 ssize_t		written;
 
	 pfd.fd = fd;
	 pfd.events = POLLOUT;

 	// write send
	 while (count > (size_t) 0) {
		 while ((written = send(fd, buf, count, MSG_NOSIGNAL)) <= 0) {
			 if (errno == EAGAIN) {
				 if (poll(&pfd, (nfds_t) 1, timeout) == 0) {
					 errno = ETIMEDOUT;
					 goto ret;
				 }
			 } else if (errno != EINTR) {
				 goto ret;
			 }
		 }
		 buf += written;
		 count -= written;
	 }
 ret:
	 return (buf - (const char *) buf_);
#if 0
bytes_sent = sendto(to->fd, data, len, 0,
                SOCK_PADDR(to), to->addr_len);
#endif
}

int udp_sendn(const int fd, const void * const buf_, 
						size_t count, const int timeout)
 {
  	struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = inet_addr("192.168.0.110");
    sa.sin_port = htons(9999);
	int ret = sendto(fd, buf_, count, 0, (struct sockaddr*)&sa, sizeof(sa));
#if 0
bytes_sent = sendto(to->fd, data, len, 0,
                SOCK_PADDR(to), to->addr_len);
#endif
}

int udp_recvn(int fd, void* const buf_, int n)
{
	 unsigned char *buf = (unsigned char *) buf_;
	 ssize_t		readnb;

	 do {
		if((readnb = recv(fd, buf, n, 0)) < 0 ) {
			if(errno == EINTR) {
				usleep(200);
				continue;
			} else if (errno == ECONNRESET) {
				printf("Client was reset, fd = [%d]\n", fd);
				return -2;
            }  else {
               printf("read error < 0, errno = %d fd = [%d]\n", errno, fd);
			   return -2;
        	}
	   	} else if (readnb == 0) {
        	printf("Client close fd break, fd = [%d]\n", fd);
			return -3;
        }	
		n -= readnb;
		buf += readnb;
	 } while (n > 0);

	 return (int) (buf - (unsigned char *) buf_);

#if 0
	bytes_recv = recvfrom(fd, data, len, 0,
						SOCK_PADDR(from), &SOCK_LEN(from));
#endif
}


 
int writen(const int fd, const void * const buf_, 
						size_t count, const int timeout)
 {
	 struct pollfd	pfd;
	 const char    *buf = (const char *) buf_;
	 ssize_t		written;
 
	 pfd.fd = fd;
	 pfd.events = POLLOUT;

 	// write send
	 while (count > (size_t) 0) {
		 while ((written = write(fd, buf, count)) <= 0) {
			 if (errno == EAGAIN) {
				 if (poll(&pfd, (nfds_t) 1, timeout) == 0) {
					 errno = ETIMEDOUT;
					 goto ret;
				 }
			 } else if (errno != EINTR) {
				 goto ret;
			 }
		 }
		 buf += written;
		 count -= written;
	 }
 ret:
	 return (buf - (const char *) buf_);
 }


