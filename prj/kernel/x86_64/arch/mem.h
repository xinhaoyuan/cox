#ifndef __KERN_ARCH_MEM_H__
#define __KERN_ARCH_MEM_H__

#include <types.h>
#include <mmu.h>
#include <arch/memlayout.h>

extern struct pseudodesc gdt_pd;
extern struct segdesc gdt[SEG_COUNT + 1];
extern pgd_t *pgdir_scratch;

int  memory_init(void);
void print_pgdir(void);
void pgflt_handler(unsigned int err, uintptr_t la, uintptr_t pc);
pte_t *get_pte(pgd_t *pgdir, uintptr_t la, bool create);

void *valloc(size_t num);
void  vfree(void *addr);

#endif
