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

static  __attribute__((constructor(101))) void before_test1()
{
 
    MSF_AGENT_LOG(DBG_INFO, "before1\n");
}
static  __attribute__((constructor(102))) void before_test2()
{
 
    MSF_AGENT_LOG(DBG_INFO, "before2\n");
}


/* 这个表示一个方法的返回值只由参数决定, 如果参数不变的话,
	就不再调用此函数，直接返回值.经过我的尝试发现还是调用了,
	后又经查资料发现要给gcc加一个-O的参数才可以.
	是对函数调用的一种优化*/
__attribute__((const)) s32 test2()
{
    return 5;
}

/* 表示函数的返回值必须被检查或使用,否则会警告*/
__attribute__((unused)) s32 test3()
{
	return 5;
}



/* 这段代码能够保证代码是内联的,因为你如果只定义内联的话,
	编译器并不一定会以内联的方式调用,
	如果代码太多你就算用了内联也不一定会内联用了这个的话会强制内联*/
static inline __attribute__((always_inline)) void test5()
{

}

__attribute__((destructor)) void after_main()  
{  
   MSF_AGENT_LOG(DBG_INFO, "after main\n\n");  
} 


static s32 agent_init(void *data, u32 len) {

    s32 rc = -1;
    s8 buf[PATH_MAX] = { 0 };

    rc = readlink("/proc/self/exe", buf, PATH_MAX-1);
    if (rc < 0 || rc >= PATH_MAX-1) {
       return -1;
    }

    MSF_AGENT_LOG(DBG_INFO, "Msf shell excute path: %s.", buf);

    return server_init();
}

static s32 agent_deinit(void *data, u32 len) {
    server_deinit();
    return 0;
}

struct svc msf_rpc_srv = {
    .init       = agent_init,
    .deinit     = agent_deinit,
    .start      = NULL,
    .stop       = NULL,
    .get_param  = NULL,
    .set_param  = NULL,
    .msg_handler= NULL,
};
