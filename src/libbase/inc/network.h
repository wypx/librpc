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
#include <stdio.h>
#include <fcntl.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/x509v3.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

enum SSLTYPE {
	SSL_NONE,
	SSL_SSLv2,
	SSL_SSLv3,
	SSL_TLSv1_0,
	SSL_TLSv1_1,
	SSL_TLSv1_2,
};


#define	PREFIX_UNIX_PATH	"/var/"		/* +5 for pid = 14 chars */


#define local_host_v4 		"127.0.0.1"
#define local_host_v6 		"[::1]"
#define local_port			"9999"

#define ADDRSTRLEN 			(INET6_ADDRSTRLEN + 9)

#define SIN(sa) 			((struct sockaddr_in *)sa)
#define SIN6(sa) 			((struct sockaddr_in6 *)sa)
#define PADDR(a) 			((struct sockaddr *)a)


/* macro should be declared in netinet/in.h even in POSIX compilation
 * but it appeared that it is not defined on some BSD system
 */
#ifndef IPPROTO_IPV6
#define IPPROTO_IPV6 41	
#endif

/* MinGW does not define IPV6_V6ONLY */
#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY 27
#endif

enum net_protocol {
    TCPV4	= IPPROTO_TCP, /**< TCP protocol */
	TCPV6,
    UDPV4	= IPPROTO_UDP, /**< UDP protocol */
    UDPV6,
    UNIX, 	/* Unix sockets*/
    MUTICAST,
};


#define IS_TCP(x) 		(x == TCPV4 || x == TCPV6)
#define IS_UDP(x) 		(x == UDPV4 || x == UDPV6)
#define IS_UNIX(x) 		(x == UNIX)
#define IS_MUTICATS(x) 	(x == MUTICAST)

#define LISTEN_MAX_FDSIZE   1024



#define  CRYPTO_LOCK_BIO   21 


/** Replacement for offsetof on platforms that don't define it. */
#ifdef offsetof
#define OFF_SETOF(type, field) offsetof(type, field)
#else
#define OFF_SETOF(type, field) ((off_t)(&((type *)0)->field))
#endif

#define sclose(fd) do { \
    int ret = close(fd); \
	if (fd != -1) { \
		while (ret != 0) { \
			if (errno != EINTR || errno == EBADF) \
				break; \
			ret = close(fd); \
		} \
		(fd) = -1;} \
	} while(0) 


#ifdef HAVE_OPENSSL

#define LIBSSL_INIT {	\
	SSL_library_init()(); \
	OpenSSL_add_all_algorithms(); \
	SSL_load_error_strings();\
	ERR_load_crypto_strings(); \
}while(0)

#define LIBSSL_CLEANUP { \
	EVP_cleanup(); \
	ERR_remove_state(0); \
	ERR_free_strings(); \
	CRYPTO_cleanup_all_ex_data(); \
}while(0)

typedef int (*verifyCert)(int, X509_STORE_CTX *);

struct tls_peer {
	int fd; 					/**< Server socket descriptor */
	struct sockaddr_storage addr; /**< Socket address */
  	enum net_protocol type; 	/**< Transport protocol used (TCP or UDP) */
	int handshake_complete; 		/**< State of the handshake */
	SSL* ssl; 
  	SSL_CTX* 	ctx_client; 	/**< SSL context for client side */
  	SSL_CTX* 	ctx_server; 	/**< SSL context for server side */
  	BIO* 		bio_fake; 		/**< Fake BIO for read operations */
    verifyCert 	ssl_cb; 		/**< Verification callback */
} __attribute__((__packed__));


#endif

struct denied_address {
  	int family; 					/**< AF family (AF_INET or AF_INET6) */
  	unsigned char addr[16]; 		/**< IPv4 or IPv6 address */
  	unsigned char mask[16]; 		/**< Network mask of the address */
  	unsigned int  port; 			/**< Network port of the address  */
  	struct denied_address* next; 	/**< For list management */
} __attribute__((__packed__));


int isipaddr(char *ip, int af_type);

int socket_ip_str(int fd, char **buf,  int size, unsigned long *len);

int socket_cork_flag(int fd, int state);
int socket_blocking(int fd);
int socket_nonblocking(int fd);

int socket_alive(int fd);
int socket_linger(int fd);
int socket_timeout(int fd, int us);
int socket_reuseaddr(int fd);
int socket_reuseport(int fd);
int socket_closeonexec(int fd);

int socket_tcp_defer_accept(int fd);
int socket_tcp_nodelay(int fd);
int socket_tcp_fastopen(int fd);


int socket_create(int domain, int type, int protocol);
int socket_bind(int fd, const struct sockaddr *addr,
                   					socklen_t addrlen, int backlog);

int recvn(int fd, void* const buf_, int n);
int sendn(const int fd, const void * const buf_, 
						size_t count, const int timeout);

int readn(int fd, void* const buf_, int n);
int writen(const int fd, const void * const buf_, 
						size_t count, const int timeout);

int udp_recvn(int fd, void* const buf_, int n);
int udp_sendn(const int fd, const void * const buf_, 
						size_t count, const int timeout);

int connect_to_unix_socket(const char* cname, const char* sname);
int connect_to_host (const char *host, const char *port);



