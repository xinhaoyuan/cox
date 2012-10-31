#ifndef __KERN_USER_H__
#define __KERN_USER_H__

#include <types.h>
#include <proc.h>
#include <timer.h>
#include <uio.h>
#include <arch/user.h>
#include <spinlock.h>

#define USER_KSTACK_DEFAULT_SIZE 8192

struct user_proc_s
{
    /* address range */
    uintptr_t start;
    uintptr_t end;

    /* arch data */
    user_proc_arch_s arch;
};

typedef struct user_proc_s  user_proc_s;
typedef struct user_proc_s *user_proc_t;

int user_proc_copy_page_to_user(user_proc_t user_proc, uintptr_t addr, uintptr_t phys, int flag);
int user_proc_copy_to_user(user_proc_t user_proc, uintptr_t addr, void *src, size_t size);
int user_proc_brk(user_proc_t user_proc, uintptr_t end);

/* filled by arch */
int user_proc_arch_init(user_proc_t user_proc, uintptr_t *start, uintptr_t *end);
int user_proc_arch_copy_page_to_user(user_proc_t user_proc, uintptr_t addr, uintptr_t phys, unsigned int flags);
int user_proc_arch_copy_to_user(user_proc_t user_proc, uintptr_t addr, void *src, size_t size);
int user_proc_arch_mmio_open(user_proc_t user_proc, uintptr_t addr, size_t size, uintptr_t *result);
int user_proc_arch_mmio_close(user_proc_t user_proc, uintptr_t addr);
int user_proc_arch_brk(user_proc_t user_proc, uintptr_t end);

struct user_thread_s
{
    proc_s        proc;
    int           pid;
    user_proc_t   user_proc;
    
    void         *tls;
    uintptr_t     tls_u;
    size_t        tls_size;

    user_thread_arch_s arch;    /* arch data */
};

typedef struct user_thread_s  user_thread_s;
typedef struct user_thread_s *user_thread_t;

#define USER_THREAD(__proc) (CONTAINER_OF(__proc, user_thread_s, proc))

int user_thread_sys_init(void);
int user_thread_init(user_thread_t thread, const char *name, int class, void *stack_base, size_t stack_size);
int user_thread_bin_exec(user_thread_t thread, void *bin, size_t bin_size);
int user_thread_create(user_thread_t thread, uintptr_t entry, uintptr_t tls_u, size_t tls_size, uintptr_t stack_ptr, user_thread_t from);

void user_thread_pgflt_handler(proc_t proc, unsigned int flags, uintptr_t la, uintptr_t pc);

void user_thread_jump(void) __attribute__((noreturn));

void user_thread_after_leave(proc_t proc);
void user_thread_before_return(proc_t proc);
void user_thread_save_context(proc_t proc, int hint);
void user_thread_restore_context(proc_t proc);

#define USER_THREAD_CONTEXT_HINT_URGENT 0x1
#define USER_THREAD_CONTEXT_HINT_LAZY   0x2

/* filled by arch */
int  user_thread_arch_state_init(user_thread_t thread, uintptr_t entry, uintptr_t stack_ptr);
void user_thread_arch_jump(void) __attribute__((noreturn));
void user_thread_arch_save_context(proc_t proc, int hint);
void user_thread_arch_restore_context(proc_t proc);

#endif
