#include "gipc_alloc.h"


/*
 * NGX_MAX_ALLOC_FROM_POOL should be (ngx_pagesize - 1), i.e. 4095 on x86.
 * On Windows NT it decreases a number of locked pages in a kernel.
 */
#define NGX_MAX_ALLOC_FROM_POOL  (gipc_pagesize - 1)

#define NGX_DEFAULT_POOL_SIZE    (16 * 1024)

#define NGX_POOL_ALIGNMENT       16
#define NGX_MIN_POOL_SIZE                                                     \
    	gipc_align((sizeof(ngx_pool_t) + 2 * sizeof(ngx_pool_large_t)),            \
              NGX_POOL_ALIGNMENT)

typedef void (*ngx_pool_cleanup_pt)(void *data);
typedef struct ngx_pool_cleanup_s  ngx_pool_cleanup_t;


/*
 * 内存池       ---  ngx_pool_s
 * 内存块数据   ---  ngx_pool_data_t
 * 大内存块     --- ngx_pool_large_s;
 */

typedef struct ngx_pool_large_s ngx_pool_large_t;

typedef struct ngx_pool_s ngx_pool_t;

struct ngx_pool_cleanup_s { 
    ngx_pool_cleanup_pt handler;

	/*内存的真正地址 回调时,将此数据传入回调函数*/
    void*				data;
    ngx_pool_cleanup_t*	next;
};

struct ngx_pool_large_s {
    ngx_pool_large_t* 	next;
    void*				alloc;
};

typedef struct {
	/* 申请过的内存的尾地址,可申请的首地址 
	   pool->d.last ~ pool->d.end 中的内存区便是可用数据区 */
    unsigned char*	last;
	/* 当前内存池节点可以申请的内存的最终位置 */
    unsigned char*	end;

	/* 下一个内存池节点ngx_pool_t,见ngx_palloc_block */
    ngx_pool_t*		next;

	/*发现从当前pool中分配内存失败四次,则使用下一个pool,见ngx_palloc_block*/ 
    unsigned int 	failed;
} ngx_pool_data_t;

/* 为了减少内存碎片的数量,并通过统一管理来减少代码中出现内存泄漏的可能性,
 * Nginx设计了ngx_pool_t内存池数据结构。
 * 图形化理解参考Nginx内存池分析:
 * http://www.linuxidc.com/Linux/2011-08/41860.htm
 */
struct ngx_pool_s {
	/* 节点数据  包含 pool 的数据区指针的结构体 
	   pool->d.last ~ pool->d.end 中的内存区便是可用数据区 */
    ngx_pool_data_t 	d;	

	/* 当前内存节点可以申请的最大内存空间 一次最多从pool中开辟的最大空间 */
    unsigned int		max;
	/* 内存池中可以申请内存的第一个节点 */
    ngx_pool_t*			current;

	//ngx_chain_t*		chain
	/* 节点中大内存块指针 pool 中指向大数据快的指针
	   大数据快是指 size > max 的数据块 */
    ngx_pool_large_t*	large;

	/* pool中指向 ngx_pool_cleanup_t 数据块的指针 
	   cleanup在ngx_pool_cleanup_add赋值 */
    ngx_pool_cleanup_t*	cleanup;
};


typedef struct {
    int     fd;
    char*	name;
} ngx_pool_cleanup_file_t;


void* ngx_alloc(unsigned int size);
void* ngx_calloc(unsigned int size);

ngx_pool_t* ngx_create_pool(unsigned int size);
void ngx_destroy_pool(ngx_pool_t* pool);
void ngx_reset_pool (ngx_pool_t* pool);
void ngx_pool_status(ngx_pool_t* pool);


void *ngx_palloc(ngx_pool_t *pool, unsigned int size);
void *ngx_pnalloc(ngx_pool_t *pool, unsigned int size);
void *ngx_prealloc(ngx_pool_t *pool, void *p, unsigned int size);
void *ngx_pcalloc(ngx_pool_t *pool, unsigned int size);
void *ngx_pmemalign(ngx_pool_t *pool, unsigned int size, unsigned int alignment);
int   ngx_pfree (ngx_pool_t *pool, void *p);


ngx_pool_cleanup_t *ngx_pool_cleanup_add(ngx_pool_t *p, unsigned int size);
void ngx_pool_run_cleanup_file(ngx_pool_t *p, int fd);
void ngx_pool_cleanup_file(void *data);
void ngx_pool_delete_file(void *data);

