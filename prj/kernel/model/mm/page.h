#ifndef __PAGE_H__
#define __PAGE_H__

#include <types.h>

/* implemented in arch */

uintptr_t page_alloc_atomic(size_t num);
void      page_free_atomic(uintptr_t addr);

#endif
