/*****************************************************************************
 * Copyright (C) 2014-2015
 * file:    epoll.c
 * author:  gozfree <gozfree@163.com>
 * created: 2015-04-27 00:59
 * updated: 2015-07-12 00:41
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/epoll.h>
#include <fcntl.h>


#include "gipc_event.h"


//http://www.cnblogs.com/Anker/archive/2013/08/17/3263780.html
/* http://blog.csdn.net/yusiguyuan/article/details/15027821
   EPOLLET:
   将EPOLL设为边缘触发(Edge Triggered)模式,
   这是相对于水平触发(Level Triggered)来说的

   EPOLL事件有两种模型：
   Edge Triggered(ET)		
   高速工作方式,错误率比较大,只支持no_block socket (非阻塞socket)
   LevelTriggered(LT)		
   缺省工作方式，即默认的工作方式,支持blocksocket和no_blocksocket,错误率比较小.

   EPOLLIN:
   listen fd,有新连接请求,对端发送普通数据

   EPOLLPRI:
   有紧急的数据可读(这里应该表示有带外数据到来)

   EPOLLERR:
   表示对应的文件描述符发生错误

   EPOLLHUP:
   表示对应的文件描述符被挂断 
   对端正常关闭(程序里close(),shell下kill或ctr+c),触发EPOLLIN和EPOLLRDHUP,
   但是不触发EPOLLERR 和EPOLLHUP.再man epoll_ctl看下后两个事件的说明,
   这两个应该是本端（server端）出错才触发的.

   EPOLLRDHUP:
   这个好像有些系统检测不到,可以使用EPOLLIN,read返回0,删除掉事件,关闭close(fd)

   EPOLLONESHOT:
   只监听一次事件,当监听完这次事件之后,
   如果还需要继续监听这个socket的话,
   需要再次把这个socket加入到EPOLL队列里

   对端异常断开连接(只测了拔网线),没触发任何事件。

   epoll的优点：
	1.支持一个进程打开大数目的socket描述符(FD)
	select 最不能忍受的是一个进程所打开的FD是有一定限制的，
	由FD_SETSIZE设置，默认值是2048。
	对于那些需要支持的上万连接数目的IM服务器来说显然太少了。
	这时候你一是可以选择修改这个宏然后重新编译内核，
	不过资料也同时指出这样会带来网络效率的下降，
	二是可以选择多进程的解决方案(传统的 Apache方案)，
	不过虽然linux上面创建进程的代价比较小，但仍旧是不可忽视的，
	加上进程间数据同步远比不上线程间同步的高效，所以也不是一种完美的方案。
	不过 epoll则没有这个限制，它所支持的FD上限是最大可以打开文件的数目，
	这个数字一般远大于2048,举个例子,在1GB内存的机器上大约是10万左右，
	具体数目可以cat /proc/sys/fs/file-max察看,一般来说这个数目和系统内存关系很大。
	
	2.IO效率不随FD数目增加而线性下降
	传统的select/poll另一个致命弱点就是当你拥有一个很大的socket集合，
	不过由于网络延时，任一时间只有部分的socket是"活跃"的，
	但是select/poll每次调用都会线性扫描全部的集合，导致效率呈现线性下降。
	但是epoll不存在这个问题，它只会对"活跃"的socket进行操作---
	这是因为在内核实现中epoll是根据每个fd上面的callback函数实现的。
	那么，只有"活跃"的socket才会主动的去调用 callback函数，
	其他idle状态socket则不会，在这点上，epoll实现了一个"伪"AIO，
	因为这时候推动力在os内核。在一些 benchmark中，
	如果所有的socket基本上都是活跃的---比如一个高速LAN环境，
	epoll并不比select/poll有什么效率，相反，如果过多使用epoll_ctl,
	效率相比还有稍微的下降。但是一旦使用idle connections模拟WAN环境,
	epoll的效率就远在select/poll之上了

	3.使用mmap加速内核与用户空间的消息传递
	这点实际上涉及到epoll的具体实现了。
	无论是select,poll还是epoll都需要内核把FD消息通知给用户空间，
	如何避免不必要的内存拷贝就很重要，在这点上，
	epoll是通过内核于用户空间mmap同一块内存实现的。
	而如果你想我一样从2.5内核就关注epoll的话，一定不会忘记手工 mmap这一步的。

	4.内核微调
	这一点其实不算epoll的优点了，而是整个linux平台的优点。
	也许你可以怀疑linux平台，但是你无法回避linux平台赋予你微调内核的能力。
	比如，内核TCP/IP协议栈使用内存池管理sk_buff结构，
	那么可以在运行时期动态调整这个内存pool(skb_head_pool)的大小--- 
	通过echoXXXX>/proc/sys/net/core/hot_list_length完成。
	再比如listen函数的第2个参数(TCP完成3次握手的数据包队列长度)，
	也可以根据你平台内存大小动态调整。
	更甚至在一个数据包面数目巨大但同时每个数据包本身大小却很小的特殊系统上
	尝试最新的NAPI网卡驱动架构。

	
  */

/* 可以注册事件, 修改事件, 删除事件 
   EPOLL_CTL_ADD EPOLL_CTL_MOD EPOLL_CTL_DEL */ 


#ifdef __ANDROID__
#define EPOLLRDHUP      (0x2000)
#endif

#define EPOLL_MAX_NEVENT    (4096)
#define MAX_SECONDS_IN_MSEC_LONG \
        (((LONG_MAX) - 999) / 1000)

#define LISTEN_EPOLLEVENTS 	10
#define LISTEN_EPOLLFDSIZE 	10


struct epoll_ctx {
    int epfd;
    int nevents;
 	struct epoll_event epev;
    struct epoll_event *events;
};

static void *epoll_init(void)
{
    int fd = -1;
    struct epoll_ctx *ec;
    fd = epoll_create(LISTEN_EPOLLFDSIZE);
    if (-1 == fd) {
        printf("errno = %d %s\n", errno, strerror(errno));
        return NULL;
    }
    ec = (struct epoll_ctx *)malloc(sizeof(struct epoll_ctx));
    if (!ec) {
        printf("malloc epoll_ctx failed!\n");
        return NULL;
    }
	memset(ec, 0, sizeof(struct epoll_ctx));
    ec->epfd = fd;
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	
    ec->nevents = LISTEN_EPOLLEVENTS;
    ec->events = (struct epoll_event *)malloc(sizeof(struct epoll_event)*LISTEN_EPOLLEVENTS);
    if (!ec->events) {
        printf("malloc epoll_event failed!\n");
        return NULL;
    }
	memset(ec->events, 0, sizeof(struct epoll_event)*LISTEN_EPOLLEVENTS);
    return ec;
}

static void epoll_deinit(void *ctx)
{
    struct epoll_ctx *ec = (struct epoll_ctx *)ctx;
    if (!ctx) {
        return;
    }
	close(ec->epfd);
    free(ec->events);
    free(ec);
	/*
	这个参数不同于select()中的第一个参数，给出最大监听的fd+1的值。
	需要注意的是，当创建好epoll句柄后，它就是会占用一个fd值，
	在linux下如果查看/proc/进程id/fd/，是能够看到这个fd的，
	所以在使用完epoll后，必须调用close()关闭，否则可能导致fd被耗尽。*/
}

static int epoll_add(struct gevent_base *eb, struct gevent *e)
{
    struct epoll_ctx *ec = (struct epoll_ctx *)eb->ctx;

    if (e->flags & EVENT_READ)
        ec->epev.events |= EPOLLIN;
    if (e->flags & EVENT_WRITE)
        ec->epev.events |= EPOLLOUT;
    if (e->flags & EVENT_ERROR)
        ec->epev.events |= EPOLLERR;
    //ec->epev.events |= EPOLLET;
	//ec->epev.events |= EPOLLRDHUP;
	//ec->epev.events |= EPOLLHUP;
    ec->epev.data.ptr = (void *)e;

    if (-1 == epoll_ctl(ec->epfd, EPOLL_CTL_ADD, e->evfd, &ec->epev)) {
        printf("errno = %d %s\n", errno, strerror(errno));
        return -1;
    }
    return 0;
}

static int epoll_mod(struct gevent_base *eb, struct gevent *e)
{
    return 0;
}


static int epoll_del(struct gevent_base *eb, struct gevent *e)
{
    struct epoll_ctx *ec = (struct epoll_ctx *)eb->ctx;
    if (-1 == epoll_ctl(ec->epfd, EPOLL_CTL_DEL, e->evfd, NULL)) {
        perror("epoll_ctl");
        return -1;
    }
    return 0;
}

static int epoll_dispatch(struct gevent_base *eb, struct timeval *tv)
{
    struct epoll_ctx *epop = (struct epoll_ctx *)eb->ctx;
    struct epoll_event *events = epop->events;
    int i, n;
    int timeout = -1;

    if (tv != NULL) {
        if (tv->tv_usec > 1000000 || tv->tv_sec > MAX_SECONDS_IN_MSEC_LONG)
            timeout = -1;
        else
            timeout = (tv->tv_sec * 1000) + ((tv->tv_usec + 999) / 1000);
    } else {
        timeout = -1;
    }
    n = epoll_wait(epop->epfd, events, epop->nevents, timeout);
    if (-1 == n) {
        if (errno != EINTR) {
            printf("epoll_wait failed %d: %s\n", errno, strerror(errno));
            return -1;
        }
        return 0;
    }
    if (0 == n) {
        printf("epoll_wait timeout\n");
        return 0;
    }
    for (i = 0; i < n; i++) {
        int what = events[i].events;
        struct gevent *e = (struct gevent *)events[i].data.ptr;

        if (what & (EPOLLHUP | EPOLLERR)) {
			 e->evcb->ev_err(e->evfd, (void *)e->evcb->args);
        } else {
            if (what & EPOLLIN) {
                e->evcb->ev_in(e->evfd, (void *)e->evcb->args);
				//events[i].events= EPOLLOUT | EPOLLET;
				events[i].events = EPOLLIN;
				epoll_ctl(epop->epfd, EPOLL_CTL_MOD, e->evfd, &epop->epev);
			
        	}
            if (what & EPOLLOUT) {
                e->evcb->ev_out(e->evfd, (void *)e->evcb->args);
				events[i].events = EPOLLIN;
				epoll_ctl(epop->epfd, EPOLL_CTL_MOD, e->evfd, &epop->epev);
        	}
            if (what & EPOLLRDHUP)
                e->evcb->ev_err(e->evfd, (void *)e->evcb->args);
        }
    }
    return 0;
}

struct gevent_ops epollops = {
    .init     = epoll_init,
    .deinit   = epoll_deinit,
    .add      = epoll_add,
    .del      = epoll_del,
    .dispatch = epoll_dispatch,
};
