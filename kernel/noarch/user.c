#define DEBUG_COMPONENT DBG_SCHED

#include <error.h>
#include <proc.h>
#include <user.h>
#include <frame.h>
#include <kmalloc.h>
#include <irq.h>
#include <string.h>
#include <arch/irq.h>
#include <algo/list.h>
#include <debug.h>
#include <ips.h>
#include <spinlock.h>

#define USER_PROC_COUNT 4096
spinlock_s  up_free_lock;
user_proc_t up_free;
user_proc_s up_pool[USER_PROC_COUNT];

#define USER_THREAD_COUNT 8192
spinlock_s    ut_free_lock;
user_thread_t ut_free;
user_thread_s ut_pool[USER_THREAD_COUNT];

void
user_proc_sys_init(void)
{
    int i;
    for (i = 0; i < USER_PROC_COUNT; ++ i)
    {
        spinlock_init(&up_pool[i].ref_lock);
        up_pool[i].ref_count = 0;
        up_pool[i].free_next = &up_pool[i + 1];
    }
    up_pool[USER_PROC_COUNT - 1].free_next = NULL;
    up_free = &up_pool[0];
    spinlock_init(&up_free_lock);
}

static user_proc_t
user_proc_create(uintptr_t *start, uintptr_t *end)
{
    user_proc_t up;
    
    int irq = __irq_save();
    spinlock_acquire(&up_free_lock);
    if ((up = up_free) != NULL)
    {
        up_free = up_free->free_next;
    }
    spinlock_release(&up_free_lock);
    __irq_restore(irq);

    if (up == NULL) return NULL;

    spinlock_init(&up->ref_lock);
    up->ref_count = 1;

    int ret;
    if ((ret = user_proc_arch_init(up, start, end)))
    {
        user_proc_put(up);
        return NULL;
    }

    up->start = *start;
    up->end   = *end;

    return up;
}

static void
user_proc_free(user_proc_t up)
{
    user_proc_arch_destroy(up);
    
    int irq = __irq_save();
    spinlock_acquire(&up_free_lock);
    up->free_next = up_free;
    up_free = up;
    spinlock_release(&up_free_lock);
    __irq_restore(irq);
}

void
user_proc_get(user_proc_t up)
{
    int irq = __irq_save();
    spinlock_acquire(&up->ref_lock);
    ++ up->ref_count;
    spinlock_release(&up->ref_lock);
    __irq_restore(irq);
}

void
user_proc_put(user_proc_t up)
{
    int irq = __irq_save();
    spinlock_acquire(&up->ref_lock);
    unsigned rc = -- up->ref_count;
    spinlock_release(&up->ref_lock);
    __irq_restore(irq);

    if (rc == 0)
        user_proc_free(up);
}

void
user_thread_sys_init(void)
{
    int i;
    for (i = 0; i < USER_THREAD_COUNT; ++ i)
    {
        spinlock_init(&ut_pool[i].ref_lock);
        ut_pool[i].ref_count = 0;
        ut_pool[i].free_next = &ut_pool[i + 1];
        ut_pool[i].tid = i + 1;
    }
    ut_pool[USER_THREAD_COUNT - 1].free_next = NULL;
    ut_free = &ut_pool[0];
    spinlock_init(&ut_free_lock);
}

static user_thread_t
user_thread_create(const char *name, int class)
{
    size_t stack_size = USER_KSTACK_DEFAULT_SIZE;
    void  *stack_base = frame_arch_kopen((stack_size + _MACH_PAGE_SIZE - 1) >> _MACH_PAGE_SHIFT);

    if (stack_base == NULL) return NULL;
    user_thread_t ut;

    int irq = __irq_save();
    spinlock_acquire(&ut_free_lock);
    if ((ut = ut_free) != NULL)
    {
        ut_free = ut_free->free_next;
    }
    spinlock_release(&ut_free_lock);
    __irq_restore(irq);

    if (ut == NULL)
    {
        frame_arch_kclose(stack_base);
        return NULL;
    }

    ut->ref_count = 1;

    proc_init(&ut->proc, name, class, (void(*)(void *))user_thread_jump, NULL, stack_base, stack_size);
    ut->proc.type  = PROC_TYPE_USER_INIT;
    ut->stack_base = stack_base;

    ut->service_context = NULL;
    ut->service_client  = NULL;
    semaphore_init(&ut->service_wait_sem, 0);
    semaphore_init(&ut->service_fill_sem, 0);

    return ut;
}

static void
user_thread_free(user_thread_t ut)
{
    user_thread_arch_destroy(ut);
    if (ut->user_proc)
        user_proc_put(ut->user_proc);
    frame_arch_kclose(ut->stack_base);

    int irq = __irq_save();
    spinlock_acquire(&ut_free_lock);
    ut->free_next = ut_free;
    ut_free = ut;
    spinlock_release(&ut_free_lock);
    __irq_restore(irq);
}

void
user_thread_get(user_thread_t ut)
{
    int irq = __irq_save();
    spinlock_acquire(&ut->ref_lock);
    ++ ut->ref_count;
    spinlock_release(&ut->ref_lock);
    __irq_restore(irq);
}

void
user_thread_put(user_thread_t ut)
{
    int irq = __irq_save();
    spinlock_acquire(&ut->ref_lock);
    unsigned rc = -- ut->ref_count;
    spinlock_release(&ut->ref_lock);
    __irq_restore(irq);

    if (rc == 0)
        user_thread_free(ut);
}

user_thread_t
user_thread_get_by_tid(int tid)
{
    if (tid < 1 || tid > USER_THREAD_COUNT)
        return NULL;

    user_thread_t ut = &ut_pool[tid - 1];
    int irq = __irq_save();
    spinlock_acquire(&ut->ref_lock);
    
    if (ut->ref_count == 0)
    {
        spinlock_release(&ut->ref_lock);
        __irq_restore(irq);
        return NULL;
    }
    
    ++ ut->ref_count;
    spinlock_release(&ut->ref_lock);
    __irq_restore(irq);

    return ut;
    
}

static int
user_thread_state_init(user_thread_t thread, uintptr_t entry, uintptr_t tls, uintptr_t stack)
{
    return user_thread_arch_state_init(thread, entry, tls, stack);
}

int
user_proc_copy_page_to_user(user_proc_t user_proc, uintptr_t addr, uintptr_t phys, unsigned int flags)
{
    return user_proc_arch_copy_page_to_user(user_proc, addr, phys, flags);
}

int
user_proc_copy_to_user(user_proc_t user_proc, uintptr_t addr, void *src, size_t size)
{
    return user_proc_arch_copy_to_user(user_proc, addr, src, size);
}

int
user_proc_brk(user_proc_t user_proc, uintptr_t end)
{
    if (end <= user_proc->start) return -E_INVAL;
    if (end & (PGSIZE - 1)) return -E_INVAL;
    int ret = user_proc_arch_brk(user_proc, end);
    if (ret == 0)
        user_proc->end = end;
    return ret;
}

user_thread_t
user_thread_create_from_bin(const char *name, void *bin, size_t bin_size)
{
    if (bin_size < sizeof(uintptr_t) * 4) return NULL;

    uintptr_t *ptr   =  (uintptr_t *)bin;  
    uintptr_t  start = *(ptr ++);
    uintptr_t  entry = *(ptr ++);
    uintptr_t  edata = *(ptr ++);
    uintptr_t  end   = *(ptr ++);

    if (PGOFF(start) || PGOFF(edata) || PGOFF(end)) return NULL;

    user_thread_t thread = user_thread_create(name, SCHED_CLASS_RR);

    uintptr_t __start = start;
    uintptr_t __end   = end;

    user_proc_t user_proc = thread->user_proc = user_proc_create(&__start, &__end);
    if (user_proc == NULL)
    {
        user_thread_put(thread);
        return NULL;
    }

    int ret;
    if ((ret = user_thread_state_init(thread, entry + __start - start, 0, 0)) != 0)
    {
        user_thread_put(thread);
        return NULL;
    }

    /* Touch the target memory space */
    uintptr_t now = start;
    while (now < end)
    {
        if (now < edata)
        {
            /* DATA */
            if ((ret = user_proc_copy_page_to_user(user_proc, now + __start - start, 0, 0)))
            {
                user_thread_put(thread);
                return NULL;
            }
        }
        else
        {
            /* BSS */
            /* XXX: on demand */
            if ((ret = user_proc_copy_page_to_user(user_proc, now + __start - start, 0, 0)))
            {
                user_thread_put(thread);
                return NULL;
            }
        }
        now += _MACH_PAGE_SIZE;
    }
    
    /* copy the binary */
    user_proc_copy_to_user(user_proc, __start, bin, bin_size);

    return thread;
}

user_thread_t
user_thread_create_from_thread(const char *name, user_thread_t from, uintptr_t entry, uintptr_t tls, uintptr_t stack)
{
    user_thread_t thread = user_thread_create(name, SCHED_CLASS_RR);
    if (thread == NULL) return NULL;
    
    int ret;
    thread->user_proc = from->user_proc;
    user_proc_get(from->user_proc);
    if ((ret = user_thread_state_init(thread, entry, tls, stack)) != 0)
    {
        user_thread_put(thread);
        return NULL;
    }

    return thread;
}

void
user_thread_jump(void)
{
    __irq_disable();
    current->type = PROC_TYPE_USER;
    user_thread_before_return(current);
    user_thread_arch_jump();
}

void
user_thread_after_leave(proc_t proc)
{
    /* assume the irq is disabled, ensuring no switch */
    
    user_thread_save_context(proc, USER_THREAD_CONTEXT_HINT_URGENT);
}

void
user_thread_before_return(proc_t proc)
{
    /* assume the irq is disabled, ensuring no switch */
    
    if (proc->sched_prev_usr != proc)
    {
        if (proc->sched_prev_usr != NULL)
            user_thread_save_context(proc->sched_prev_usr, USER_THREAD_CONTEXT_HINT_LAZY);
        user_thread_restore_context(proc);
    }
}

void
user_thread_pgflt_handler(proc_t proc, unsigned int flag, uintptr_t la, uintptr_t pc)
{
    /* Here to process user page faults */
}

void
user_thread_save_context(proc_t proc, int hint)
{ user_thread_arch_save_context(proc, hint); }

void
user_thread_restore_context(proc_t proc)
{ user_thread_arch_restore_context(proc); }
