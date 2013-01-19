#ifndef _MEM_ALLOCATE_H
#define _MEM_ALLOCATE_H

#include <malloc.h>
#include "mem2.h"

extern __inline__ void* mem_alloc (size_t size) {
	return MEM2_lo_alloc(size);
}

extern __inline__ void* mem_calloc (size_t count, size_t size) {
	void *p = MEM2_lo_alloc(count * size);
	if(p) {
		memset(p, 0, count * size);
	}
	return p;
}

extern __inline__ void* mem_realloc (void *p, size_t size) {
	return MEM2_lo_realloc(p, size);
}

extern __inline__ void* mem_align (size_t a, size_t size) {
	return MEM2_lo_alloc(size);
}

extern __inline__ void mem_free (void* mem) {
	MEM2_lo_free(mem);
}

#endif /* _MEM_ALLOCATE_H */
