#ifndef __KERN_ARCH_PAGE_H__
#define __KERN_ARCH_PAGE_H__

#include <types.h>

struct page_arch_s
{
	int foo;
};

typedef struct page_arch_s page_arch_s;
typedef page_arch_s *page_arch_t;

#endif
