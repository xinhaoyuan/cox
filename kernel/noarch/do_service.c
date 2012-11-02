#define DEBUG_COMPONENT DBG_IO

#include <debug.h>
#include <service.h>
#include <do_service.h>
#include <proc.h>
#include <user.h>
#include <ips.h>
#include <arch/irq.h>
#include <error.h>

static int  do_proc_brk(uintptr_t end);
static int  do_thread_create(uintptr_t entry, uintptr_t tls, uintptr_t stack);
static void do_thread_exit(int code);

void
do_service(service_context_t ctx)
{
    user_thread_t ut = USER_THREAD(current);
    uintptr_t dest = SERVICE_ARG0_GET(ctx);
    
    if (dest == 0)
    {
        int func = SERVICE_ARG1_GET(ctx);
        switch (func)
        {
        case SERVICE_SYS_DEBUG_PUTC:
            debug_putc(SERVICE_ARG2_GET(ctx));
            break;
        case SERVICE_SYS_LISTEN:
            ut->service_client = NULL;
            /* Listen for any request */
            semaphore_release(&ut->service_wait_sem);
            semaphore_acquire(&ut->service_fill_sem, NULL);
            service_context_transfer(ctx, ut->service_client->service_context);
            SERVICE_ARG0_SET(ctx, ut->service_client->tid);
            ut->service_client->service_context = NULL;
            break;
        case SERVICE_SYS_PROC_BRK:
            SERVICE_ARG0_SET(ctx, do_proc_brk(SERVICE_ARG2_GET(ctx)));
            break;
        case SERVICE_SYS_THREAD_CREATE:
            SERVICE_ARG0_SET(ctx, do_thread_create(SERVICE_ARG2_GET(ctx), SERVICE_ARG3_GET(ctx), SERVICE_ARG4_GET(ctx)));
            break;
        case SERVICE_SYS_THREAD_EXIT:
            do_thread_exit(SERVICE_ARG2_GET(ctx));
            break;
        case SERVICE_SYS_THREAD_YIELD:
            yield();
            break;
        }
    }
    else
    {
        int tid = SERVICE_ARG0_GET(ctx);
        user_thread_t target = user_thread_get_by_tid(tid);
        
        if (target == NULL)
        {
            SERVICE_ARG0_SET(ctx, E_INVAL);
            return;
        }

        if (semaphore_try_acquire(&target->service_wait_sem) == 0)
        {
            SERVICE_ARG0_SET(ctx, E_BUSY);            
            user_thread_put(target);
            return;
        }
        
        ut->service_context = ctx;
        target->service_client = ut;
        semaphore_release(&target->service_fill_sem);
        while (ut->service_context != NULL) yield();
        user_thread_put(target);

        SERVICE_ARG0_SET(ctx, 0);
    }
}

int
do_proc_brk(uintptr_t end)
{
    /* XXX */
    return 0;
}

int 
do_thread_create(uintptr_t entry, uintptr_t tls, uintptr_t stack)
{
    user_thread_t ut = USER_THREAD(current);
    user_thread_t thread = NULL;
    if (ut)
    {
        thread = user_thread_create_from_thread(ut->proc.name, ut, entry, tls, stack);
        if (thread != NULL)
            proc_notify(&thread->proc);
    }
    return thread == NULL ? 0 : thread->tid;
}

void
do_thread_exit(int code)
{
    /* XXX */
}
