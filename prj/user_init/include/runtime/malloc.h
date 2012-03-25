#ifndef __MALLOC_H__
#define __MALLOC_H__

#include <types.h>

int   malloc_init(void);
void *malloc(size_t size);
void  free(void *ptr);

#endif
