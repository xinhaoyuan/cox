#include <atom.h>
#include <string.h>

#include <mm/page.h>
#include <lib/low_io.h>
#include <arch/mem.h>
#include <arch/lapic.h>
#include <arch/mmu.h>
#include <arch/memlayout.h>

#include <arch/local.h>

extern char pls_start[];
extern char pls_end[];

void
pls_init(void)
{
	size_t pls_pages = (pls_end - pls_start + PGSIZE - 1) >> PGSHIFT;
	void *pls;

	pls = VADDR_DIRECT(page_alloc_atomic(pls_pages));

	/* copy the initial data */
	memmove(pls, pls_start, pls_end - pls_start);
	
	uintptr_t base = (uintptr_t)pls - (uintptr_t)pls_start;

	/* set and load segment */
	gdt[SEG_PLS(lapic_id())] = SEG_BASE(STA_W, base, DPL_KERNEL);
	__asm__ __volatile__("movw %w0, %%fs" : : "a"(GD_PLS(lapic_id())));
}
