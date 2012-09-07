#ifndef __KERN_ARCH_USER_H__
#define __KERN_ARCH_USER_H__

#include <uio.h>
#include <proc.h>
#include <arch/memlayout.h>
#include <mmu.h>
#include <ips.h>

struct user_thread_arch_s
{
    struct trapframe *tf;
    uintptr_t init_entry;
};

typedef struct user_thread_arch_s  user_thread_arch_s;
typedef struct user_thraad_arch_s *user_thread_arch_t;

struct user_proc_arch_s
{
    mutex_s pgflt_lock;
    uintptr_t cr3;
    pgd_t *pgdir;

    void *mmio_root;
};

typedef struct user_proc_arch_s  user_proc_arch_s;
typedef struct user_proc_arch_s *user_proc_arch_t;

#endif
