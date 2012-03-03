#ifndef __KERN_ARCH_MEM_H__
#define __KERN_ARCH_MEM_H__

#include <types.h>
#include <driver/mmu.h>
#include <driver/memlayout.h>

extern struct pseudodesc gdt_pd;
extern struct segdesc gdt[SEG_COUNT + 1];

int  mem_init(void);
void print_pgdir(void);

#endif
