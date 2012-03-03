#ifndef __KERN_MALLOC_H__
#define __KERN_MALLOC_H__

#include <types.h>

int   malloc_init(void);

void *kmalloc(size_t size);
void  kfree(void *ptr);

#endif
