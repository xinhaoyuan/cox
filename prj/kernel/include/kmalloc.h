#ifndef __KERN_KMALLOC_H__
#define __KERN_KMALLOC_H__

#include <types.h>

int   kmalloc_sys_init(void);
void *kmalloc(size_t size, unsigned int flags);
void  kfree(void *ptr);


#endif
