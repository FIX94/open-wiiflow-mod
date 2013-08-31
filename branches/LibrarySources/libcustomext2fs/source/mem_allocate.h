#ifndef _MEM_ALLOCATE_H
#define _MEM_ALLOCATE_H

#include "mem2.hpp"

extern __inline__ void* mem_alloc (unsigned int size) {
    return MEM2_alloc(size);
}

extern __inline__ void* mem_calloc (unsigned int count, unsigned int size) {
    void *p = MEM2_alloc(count * size);
    if(p) memset(p, 0, count * size);
    return p;
}

extern __inline__ void* mem_realloc (void *p, unsigned int size) {
    return MEM2_realloc(p, size);
}

extern __inline__ void* mem_align (unsigned int a, unsigned int size) {
    #ifdef __wii__
    return MEM2_memalign(a, size);
    #else
    return MEM2_alloc(size);
    #endif
}

extern __inline__ void mem_free (void* mem) {
    MEM2_free(mem);
}

#endif /* _MEM_ALLOCATE_H */
