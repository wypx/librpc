#include "gipc_alloc.h"

/* ngx_os_init返回一个分页的大小,单位为字节(Byte).
 * 该值为系统的分页大小,不一定会和硬件分页大小相同 
 * ngx_pagesize为4M，ngx_pagesize_shift应该为12
 * ngx_pagesize进行移位的次数,见for (n = ngx_pagesize; 
 * n >>= 1; ngx_pagesize_shift++) { void }
 */
//#define ngx_os_pagesize (getpagesize() - 1)
unsigned int  gipc_pagesize = 4096; 
unsigned int  gipc_pagesize_shift; 

/* 如果能知道CPU cache行的大小,那么就可以有针对性地设置内存的对齐值,
 * 这样可以提高程序的效率.Nginx有分配内存池的接口,Nginx会将内存池边界
 * 对齐到 CPU cache行大小 32位平台,ngx_cacheline_size=32
 */
//unsigned int  gipc_cacheline_size = 32;


void* gipc_alloc(unsigned int size)
{
    void* p;

    p = malloc(size);
	if ( !p ) {
		printf("malloc size [%d] failed\n", size);
	}

    return p;
}


void* gipc_calloc(unsigned int size)
{
    void* p;

    p = gipc_alloc(size);

    if (p) {
        gipc_memzero(p, size);
    }

    return p;
}

void* ngx_realloc(void *p, unsigned int size){

    if(p) {
        return realloc (p, size);
    }

    return NULL;

}

#if (GIPC_HAVE_POSIX_MEMALIGN)

void* gipc_memalign(unsigned int alignment, unsigned int size)
{
    void* p   = NULL;
    int   err = -1;

	/*void* p = memalign(alignment, size) */
	
    err = posix_memalign(&p, alignment, size);
	if ( err ) {
		printf("posix_memalign(%uz, %uz) failed\n", alignment, size);
	}

    return p;
}

#elif (GIPC_HAVE_MEMALIGN)

void* gipc_memalign(unsigned int alignment, unsigned int size) {

	void* p;

    p = memalign(alignment, size);
	if ( !p ) {
		printf("memalign(%uz, %uz) failed\n", alignment, size);
	}

	return p;
}

#endif

