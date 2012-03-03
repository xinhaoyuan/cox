#include <atom.h>
#include <string.h>

#include <mm/page.h>
#include <lib/low_io.h>
#include <driver/mem.h>
#include <driver/lapic.h>
#include <driver/mmu.h>
#include <driver/memlayout.h>

#include <proc/local.h>

extern char pls_start[];
extern char pls_end[];

void
pls_init(void)
{
	size_t pls_pages = (pls_end - pls_start + PGSIZE - 1) >> PGSHIFT;
	void *pls;

	/* XXX: better lock here */
	static int lock = 0;
	while (__xchg32(&lock, 1) == 1) ;
	
	pls = VADDR_DIRECT(page_alloc_atomic(pls_pages));
	
	__xchg32(&lock, 0);

	/* copy the initial data */
	memmove(pls, pls_start, pls_end - pls_start);
	
	uintptr_t base = (uintptr_t)pls - (uintptr_t)pls_start;

	/* set and load segment */
	gdt[SEG_PLS(lapic_id())] = SEG_BASE(STA_W, base, DPL_KERNEL);
	__asm__ __volatile__("movw %w0, %%fs" : : "a"(GD_PLS(lapic_id())));
}
