#include "common.h"
/*
 * Linux has memalign() or posix_memalign()
 * Solaris has memalign()
 * FreeBSD 7.0 has posix_memalign(), besides, early version's malloc()
 * aligns allocations bigger than page size at the page boundary
 */
#if (GIPC_HAVE_POSIX_MEMALIGN || GIPC_HAVE_MEMALIGN)

void* gipc_memalign(unsigned int alignment, unsigned int size);

#else

#define gipc_memalign(alignment, size)  gipc_alloc(size)

#endif


void* gipc_alloc(unsigned int size);
void* gipc_calloc(unsigned int size);
void* gipc_realloc(void *p, unsigned int size);


extern unsigned int  gipc_pagesize;
extern unsigned int  gipc_pagesize_shift;
extern unsigned int  gipc_cacheline_size;
