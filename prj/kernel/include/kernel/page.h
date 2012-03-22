#ifndef __KERN_PAGE_H__
#define __KERN_PAGE_H__

#include <types.h>
#include <panic.h>
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
#ifndef PAGE_ARCH_MAP
#include <mmu.h>
#define PHYS_TO_PAGE(addr) ({ size_t idx = (addr) >> PGSHIFT; idx < pages_count ? pages + idx : NULL; })
#define PAGE_TO_PHYS(page) ({ size_t idx = (page) - pages; \
			if (idx >= pages_count) { panic("PAGE_TO_PHYS out of range"); } \
			idx << PGSHIFT; })
#endif
/* or the arch defines the translation */

void page_alloc_init_struct(size_t pcount, void*(*init_alloc)(size_t));
void page_alloc_init_layout(int(*layout)(uintptr_t pagenum));

page_t page_alloc_atomic(size_t num);
void   page_free_atomic(page_t addr);

#endif
