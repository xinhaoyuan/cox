#ifndef __MMIO_H__
#define __MMIO_H__

#include <types.h>

/* implemented in arch */

volatile void *mmio_open(uintptr_t addr, size_t size);
void mmio_close(volatile void *addr);

#endif
