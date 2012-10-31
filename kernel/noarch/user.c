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

int
user_thread_init(user_thread_t thread, const char *name, int class, void *stack_base, size_t stack_size)
{
    proc_init(&thread->proc, name, class, (void(*)(void *))user_thread_jump, NULL, stack_base, stack_size);
    thread->proc.type = PROC_TYPE_USER_INIT;
    return 0;
}

static int user_proc_init(user_proc_t user_proc, uintptr_t *start, uintptr_t *end);
static int user_thread_state_init(user_thread_t proc, uintptr_t entry, uintptr_t tls, size_t tls_size, uintptr_t stack_ptr);

int
user_thread_bin_exec(user_thread_t thread, void *bin, size_t bin_size)
{
    if (bin_size < sizeof(uintptr_t) * 4) return -E_INVAL;
    
    uintptr_t *ptr   =  (uintptr_t *)bin;  
    uintptr_t  start = *(ptr ++);
    uintptr_t  entry = *(ptr ++);
    uintptr_t  edata = *(ptr ++);
    uintptr_t  end   = *(ptr ++);

    if (thread->proc.type != PROC_TYPE_USER_INIT) return -E_INVAL;
    if (PGOFF(start) || PGOFF(edata) || PGOFF(end)) return -E_INVAL;
    
    user_proc_t user_proc;
    if (thread->user_proc != NULL) return -E_INVAL;
    if ((user_proc = kmalloc(sizeof(user_proc_s))) == NULL) return -E_NO_MEM;
    thread->user_proc = user_proc;

    size_t   tls_size = 2 * _MACH_PAGE_SIZE;
    uintptr_t __start = start;
    uintptr_t __end   = end + tls_size;

    int ret;
    if ((ret = user_proc_init(user_proc, &__start, &__end)) != 0) return ret;
    if ((ret = user_thread_state_init(thread, entry + __start - start, __end - tls_size, tls_size, 0)) != 0) return ret;

    /* Touch the target memory space */
    uintptr_t now = start;
    while (now < end)
    {
        if (now < edata)
        {
            /* DATA */
            if ((ret = user_proc_copy_page_to_user(user_proc, now + __start - start, 0, 0))) return ret;
        }
        else
        {
            /* BSS */
            /* XXX: on demand */
            if ((ret = user_proc_copy_page_to_user(user_proc, now + __start - start, 0, 0))) return ret;
        }
        now += _MACH_PAGE_SIZE;
    }
    
    /* copy the binary */
    user_proc_copy_to_user(user_proc, __start, bin, bin_size);

    return 0;
}

int
user_thread_exec(user_thread_t thread, uintptr_t entry, uintptr_t start, uintptr_t end)
{
    if (thread->proc.type != PROC_TYPE_USER_INIT) return -E_INVAL;
    if (PGOFF(start) || PGOFF(end)) return -E_INVAL;
    
    user_proc_t   user_proc;
    
    if (thread->user_proc != NULL) return -E_INVAL;
    if ((user_proc = kmalloc(sizeof(user_proc_s))) == NULL) return -E_NO_MEM;
    thread->user_proc = user_proc;

    size_t   tls_size = 2 * _MACH_PAGE_SIZE;
    uintptr_t __start = start;
    uintptr_t __end   = end + tls_size;

    int ret;
    if ((ret = user_proc_init(user_proc, &__start, &__end)) != 0) return ret;
    if ((ret = user_thread_state_init(thread, entry + __start - start, __end - tls_size, tls_size, 0)) != 0) return ret;
    
    return 0;
}

int
user_thread_create(user_thread_t thread, uintptr_t entry, uintptr_t tls_u, size_t tls_size, uintptr_t stack_ptr, user_thread_t from)
{
    if (thread->proc.type != PROC_TYPE_USER_INIT) return -E_INVAL;
    if (PGOFF(tls_u) || PGOFF(tls_size) || tls_size < 2 * _MACH_PAGE_SIZE) return -E_INVAL;

    int ret;
    
    thread->user_proc = from->user_proc;
    
    if ((ret = user_thread_state_init(thread, entry, tls_u, tls_size, stack_ptr)) != 0) return ret;

    return 0;
}


int
user_proc_init(user_proc_t user_proc, uintptr_t *start, uintptr_t *end)
{
    int ret;
        
    if ((ret = user_proc_arch_init(user_proc, start, end)))
    {
        return ret;
    }

    user_proc->start = *start;
    user_proc->end =   *end;

    return 0;
}

int
user_thread_state_init(user_thread_t thread, uintptr_t entry, uintptr_t tls_u, size_t tls_size, uintptr_t stack_ptr)
{
    thread->tls_u = tls_u;
    thread->tls_size = tls_size;

    /* Touch the memory */
    int i;
    for (i = 0; i < (tls_size >> _MACH_PAGE_SHIFT); ++ i)
        user_proc_arch_copy_page_to_user(thread->user_proc, thread->tls_u + (i << _MACH_PAGE_SHIFT), 0, 0);
    return user_thread_arch_state_init(thread, entry, stack_ptr);
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
