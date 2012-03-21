#ifndef __KERN_ARCH_USER_H__
#define __KERN_ARCH_USER_H__

#include <user/io.h>
#include <proc.h>
#include <arch/memlayout.h>
#include <arch/mmu.h>

struct user_thread_arch_s
{
	struct trapframe *tf;
	uintptr_t init_entry;
};

typedef struct user_thread_arch_s  user_thread_arch_s;
typedef struct user_thraad_arch_s *user_thread_arch_t;

struct user_mm_arch_s
{
	uintptr_t cr3;
	pgd_t *pgdir;

	void *mmio_root;
};

typedef struct user_mm_arch_s  user_mm_arch_s;
typedef struct user_mm_arch_s *user_mm_arch_t;

#endif
