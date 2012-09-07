#ifndef __PAGE_H__
#define __PAGE_H__

#include <types.h>

void  page_init(void *start, void *end);

void *palloc(size_t num);
void  pfree(void *addr);

void *sbrk(size_t amount);
int   brk(void *end);

#endif
