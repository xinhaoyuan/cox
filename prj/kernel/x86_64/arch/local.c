#include <cpu.h>
#include <atom.h>
#include <string.h>

#include <page.h>
#include <lib/low_io.h>
#include <arch/mmu.h>
#include <arch/memlayout.h>
#include <arch/local.h>

#include "mem.h"
#include "lapic.h"

extern char pls_start[];
extern char pls_end[];

void
pls_init(void)
{
	size_t pls_pages = (pls_end - pls_start + PGSIZE - 1) >> PGSHIFT;
	void *pls;

	pls = VADDR_DIRECT(PAGE_TO_PHYS(page_alloc_atomic(pls_pages)));
	
	/* copy the initial data */
	memmove(pls, pls_start, pls_end - pls_start);
	
	uintptr_t base = (uintptr_t)pls - (uintptr_t)pls_start;

	/* set and load segment */
	gdt[SEG_PLS(lapic_id())] = SEG(STA_W, DPL_KERNEL | 3);
	gdt[SEG_TLS(lapic_id())] = SEG(STA_W, DPL_KERNEL | 3);
	__asm__ __volatile__("movw %w0, %%fs" : : "a"(GD_PLS(lapic_id()) | 3));
	__asm__ __volatile__("movw %w0, %%gs" : : "a"(GD_TLS(lapic_id()) | 3));
	/* write the 64-bit segment base by msr */		
	__write_msr(0xC0000100, base);

	uintptr_t test;
	__asm__ __volatile__("mov %%fs:(%1), %0": "=r"(test) : "r"((uintptr_t)&base - base));
	if (base != test)
	{
		cprintf("PANIC: processor local storage failed to initailize.", base, test);
		while (1) asm volatile("pause");
	}
}
