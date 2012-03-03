#ifndef __KERN_ARCH_BOOT_ALLOC_H__
#define __KERN_ARCH_BOOT_ALLOC_H__

#include <types.h>

void  boot_alloc_init(void);
void *boot_alloc(uintptr_t size, uintptr_t align, int verbose);
void  boot_alloc_get_area(uintptr_t *start, uintptr_t *end);

#endif
