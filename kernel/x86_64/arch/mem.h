#ifndef __KERN_ARCH_MEM_H__
#define __KERN_ARCH_MEM_H__

#include <types.h>
#include <asm/mmu.h>
#include <arch/memlayout.h>
#include "seg.h"

struct user_proc_s;

extern struct pseudodesc gdt_pd;
extern struct segdesc gdt[SEG_COUNT];
extern pgd_t *pgdir_kernel;

void   lgdt(struct pseudodesc *pd);
void   print_pgdir(void);
void   pgflt_handler(unsigned int err, uintptr_t la, uintptr_t pc);
pte_t *get_pte(struct user_proc_s *up, pgd_t *pgdir, uintptr_t la, int create);

int    mem_init(void);

/* allocate for virtual address space */
void   valloc_early_init_struct(void);
void  *valloc(size_t num);
void   vfree(void *addr);

#endif
