// 2 MEM2 allocators, one for general purpose, one for covers
// Aligned and padded to 32 bytes, as required by many functions

#ifndef __MEM2_H_
#define __MEM2_H_

#ifdef __cplusplus
extern "C"
{
#endif

void *MEM2_lo_alloc(unsigned int s);
void *MEM2_lo_realloc(void *p, unsigned int s);
void MEM2_lo_free(void *p);

#ifdef __cplusplus
}
#endif

#endif // !defined(__MEM2_H_)
