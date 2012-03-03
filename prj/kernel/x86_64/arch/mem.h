#ifndef __KERN_ARCH_MEM_H__
#define __KERN_ARCH_MEM_H__

#include <types.h>
#include <arch/mmu.h>
#include <arch/memlayout.h>

extern struct pseudodesc gdt_pd;
extern struct segdesc gdt[SEG_COUNT + 1];

int  mem_init(void);
void print_pgdir(void);
void pgflt_handler(unsigned int err, uintptr_t la);

#endif
