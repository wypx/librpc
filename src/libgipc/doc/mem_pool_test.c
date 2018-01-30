#include <stdlib.h>
#include <unistd.h>
#include "palloc.h"

#include <sys/utsname.h>


unsigned int  ngx_cacheline_size = 32;

int ngx_osinfo() {

	struct utsname	u;   
	if (uname(&u) == -1) {
	   return -1;
	}

	printf("u.sysname:%s\n", u.sysname); //当前操作系统名
	printf("u.nodename:%s\n", u.nodename); //网络上的名称
	printf("u.release:%s\n", u.release); //当前发布级别
	printf("u.version:%s\n", u.version); //当前发布版本
	printf("u.machine:%s\n", u.machine); //当前硬件体系类型
	printf("u.__domainname:%s\n", u.__domainname); //当前硬件体系类型
	
#if _UTSNAME_DOMAIN_LENGTH - 0
#ifdef __USE_GNU
    printf("u.domainname::%s\n ", u.domainname);
    //char domainname[_UTSNAME_DOMAIN_LENGTH]; //当前域名
#else
    printf("u.__domainname::%s\n", u.__domainname);
    //char __domainname[_UTSNAME_DOMAIN_LENGTH];
#endif
#endif


	//获取系统中可用的 CPU 数量, 没有被激活的 CPU 则不统计 在内, 例如热添加后还没有激活的. 
	int ngx_ncpu = sysconf(_SC_NPROCESSORS_ONLN); 
	printf("ngx_ncpu :%d\n", ngx_ncpu); 


	int ngx_pagesize = getpagesize();
	printf("ngx_pagesize :%d\n", ngx_pagesize); 

	return 0;
}



#if (( __i386__ || __amd64__ ) && ( __GNUC__ || __INTEL_COMPILER ))

static inline void ngx_cpuid(unsigned int i, unsigned int *buf);

#if ( __i386__ )

static inline void
ngx_cpuid(unsigned int i, unsigned int *buf)
{

    /*
     * we could not use %ebx as output parameter if gcc builds PIC,
     * and we could not save %ebx on stack, because %esp is used,
     * when the -fomit-frame-pointer optimization is specified.
     */

    __asm__ (

    "    mov    %%ebx, %%esi;  "

    "    cpuid;                "
    "    mov    %%eax, (%1);   "
    "    mov    %%ebx, 4(%1);  "
    "    mov    %%edx, 8(%1);  "
    "    mov    %%ecx, 12(%1); "

    "    mov    %%esi, %%ebx;  "

    : : "a" (i), "D" (buf) : "ecx", "edx", "esi", "memory" );
}


#else /* __amd64__ */


static inline void
ngx_cpuid(unsigned int i, unsigned int *buf)
{
    unsigned int  eax, ebx, ecx, edx;

    __asm__ (

        "cpuid"

    : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) : "a" (i) );

    buf[0] = eax;
    buf[1] = ebx;
    buf[2] = edx;
    buf[3] = ecx;
}


#endif


void ngx_cpuinfo(void) {
	
	  unsigned char	  *vendor;
	  unsigned int	  vbuf[5], cpu[4], model;
	
	  vbuf[0] = 0;
	  vbuf[1] = 0;
	  vbuf[2] = 0;
	  vbuf[3] = 0;
	  vbuf[4] = 0;
	
	  ngx_cpuid(0, vbuf);
	
	  vendor = (unsigned char *) &vbuf[1];
	
	  if (vbuf[0] == 0) {
		  return;
	  }
	
	  ngx_cpuid(1, cpu);

	  printf("vendor : %s\n", vendor);
	  printf("cpu[0] : %d\n", cpu[0]);
	  printf("cpu[1] : %d\n", cpu[1]);
	  printf("cpu[2] : %d\n", cpu[2]);
	  printf("cpu[3] : %d\n", cpu[3]);

	  if (strcmp(vendor, "GenuineIntel") == 0) {

        switch ((cpu[0] & 0xf00) >> 8) {

        /* Pentium */
        case 5:
            ngx_cacheline_size = 32;
            break;

        /* Pentium Pro, II, III */
        case 6:
            ngx_cacheline_size = 32;

            model = ((cpu[0] & 0xf0000) >> 8) | (cpu[0] & 0xf0);

            if (model >= 0xd0) {
                /* Intel Core, Core 2, Atom */
                ngx_cacheline_size = 64;
            }

            break;

        /*
         * Pentium 4, although its cache line size is 64 bytes,
         * it prefetches up to two cache lines during memory read
         */
        case 15:
            ngx_cacheline_size = 128;
            break;
        }
	  }else if (strcmp(vendor, "AuthenticAMD") == 0) {
        ngx_cacheline_size = 64;
    }

}

#else


void
ngx_cpuinfo(void)
{
	printf("ngx_cpuinfo not x86\n");
}

#endif


int main(){

   ngx_osinfo();

   ngx_cpuinfo();

   ngx_pool_t *pool = ngx_create_pool(1024);

	printf("************ngx_create_pool**********************************\n");  

    int i = 0;
	ngx_pool_status(pool);

    for(i = 0;i < 3;i++){

        char* p1 = ngx_pcalloc(pool, 512);
		if(p1 == NULL) {
			 printf("p1 alloc failed ...\n");
			 ngx_destroy_pool(pool);
		}

		sleep(2);
		ngx_pool_status(pool);
		
		char* p2 = ngx_pcalloc(pool, 64);
		if(p2 == NULL) {
			 printf("p2 alloc failed ...\n");
			 ngx_destroy_pool(pool);
		}

		sleep(2);
		ngx_pool_status(pool);
    }

    sleep(5);
	ngx_pool_status(pool);

	ngx_reset_pool(pool);
	ngx_pool_status(pool);

	sleep(5);

    ngx_destroy_pool (pool);
	
	ngx_pool_status(pool);

    sleep(5);
}

