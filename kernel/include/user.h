#ifndef __KERN_USER_H__
#define __KERN_USER_H__

#include <types.h>
#include <proc.h>
#include <timer.h>
#include <uio.h>
#include <arch/user.h>
#include <ips.h>
#include <spinlock.h>
#include <arch/do_service.h>

#define USER_KSTACK_DEFAULT_SIZE (1 << 13) // 8 KB 

typedef struct user_proc_s *user_proc_t;
typedef struct user_proc_s
{
    spinlock_s ref_lock;
    unsigned   ref_count;

    user_proc_t free_next;
    
    /* address range */
    uintptr_t start;
    uintptr_t end;

    /* arch data */
    user_proc_arch_s arch;
} user_proc_s;

void user_proc_get(user_proc_t up);
void user_proc_put(user_proc_t up);

void user_proc_sys_init(void);
int user_proc_copy_page_to_user(user_proc_t user_proc, uintptr_t addr, uintptr_t phys, unsigned int flags);
int user_proc_copy_to_user(user_proc_t user_proc, uintptr_t addr, void *src, size_t size);
int user_proc_brk(user_proc_t user_proc, uintptr_t end);

/* filled by arch */
int user_proc_arch_init(user_proc_t user_proc, uintptr_t *start, uintptr_t *end);
void user_proc_arch_destroy(user_proc_t user_proc);
int user_proc_arch_copy_page_to_user(user_proc_t user_proc, uintptr_t addr, uintptr_t phys, unsigned int flags);
int user_proc_arch_copy_to_user(user_proc_t user_proc, uintptr_t addr, void *src, size_t size);
int user_proc_arch_mmio_open(user_proc_t user_proc, uintptr_t addr, size_t size, uintptr_t *result);
int user_proc_arch_mmio_close(user_proc_t user_proc, uintptr_t addr);
int user_proc_arch_brk(user_proc_t user_proc, uintptr_t end);

typedef struct user_thread_s *user_thread_t;
typedef struct user_thread_s
{
    spinlock_s    ref_lock;
    unsigned      ref_count;

    int           tid;
    union
    {
        user_thread_t free_next;
        struct
        {
            proc_s        proc;
            void         *stack_base;
            size_t        stack_size;
            user_proc_t   user_proc;
        };
    };
        
    /* service */
    volatile service_context_t service_context;
    volatile user_thread_t     service_client;
    semaphore_s                service_wait_sem;
    semaphore_s                service_fill_sem;

    user_thread_arch_s arch;    /* arch data */
} user_thread_s;

#define USER_THREAD(__proc) (CONTAINER_OF(__proc, user_thread_s, proc))

void user_thread_get(user_thread_t ut);
void user_thread_put(user_thread_t ut);
user_thread_t user_thread_get_by_tid(int tid);

void user_thread_sys_init(void);
user_thread_t user_thread_create_from_bin(const char *name, void *bin, size_t bin_size);
user_thread_t user_thread_create_from_thread(const char *name, user_thread_t from, uintptr_t entry, uintptr_t tls, uintptr_t stack);

void user_thread_pgflt_handler(proc_t proc, unsigned int flags, uintptr_t la, uintptr_t pc);

void user_thread_jump(void) __attribute__((noreturn));

void user_thread_after_leave(proc_t proc);
void user_thread_before_return(proc_t proc);
void user_thread_save_context(proc_t proc, int hint);
void user_thread_restore_context(proc_t proc);

#define USER_THREAD_CONTEXT_HINT_URGENT 0x1
#define USER_THREAD_CONTEXT_HINT_LAZY   0x2

/* filled by arch */
void user_thread_arch_destroy(user_thread_t user_thread);
int  user_thread_arch_state_init(user_thread_t thread, uintptr_t entry, uintptr_t tls, uintptr_t stack);
void user_thread_arch_jump(void) __attribute__((noreturn));
void user_thread_arch_save_context(proc_t proc, int hint);
void user_thread_arch_restore_context(proc_t proc);

#endif
