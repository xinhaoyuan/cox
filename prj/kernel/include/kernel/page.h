#ifndef __PAGE_H__
#define __PAGE_H__

#include <types.h>
#include <arch/page.h>

struct page_s
{
	int ref_count;
	uintptr_t region;

	page_arch_s arch;
};

typedef struct page_s page_s;
typedef page_s *page_t;

extern size_t pages_count;
extern page_t pages;

#define PAGE_REGION_HEAD(page) ({ page_t p = (page); p->region & 1 ? pages + (p->region & ~1) : p; })
#define PAGE_REGION_SIZE(page) ({ page_t p = (page); p->region & 1 ? (pages[(p->region & ~1)]->region >> 1) : (p->region >> 1); })
#define PAGE(addr) ({ size_t idx = addr >> PGSHIFT; idx < pages_count ? pages[idx] : NULL; })

uintptr_t page_alloc_atomic(size_t num);
void      page_free_atomic(uintptr_t addr);

/* implemented in arch */

uintptr_t page_arch_alloc(size_t num);
void      page_arch_free(uintptr_t addr);

#endif
