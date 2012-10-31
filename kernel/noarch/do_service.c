#define DEBUG_COMPONENT DBG_IO

#include <debug.h>
#include <service.h>
#include <do_service.h>
#include <proc.h>
#include <user.h>
#include <ips.h>
#include <arch/irq.h>

static int  do_thread_create(uintptr_t entry, uintptr_t tls, uintptr_t stack);
static void do_thread_exit(int code);

void
do_service(service_context_t ctx)
{
    user_thread_t ut = USER_THREAD(current);
    uintptr_t dest = SERVICE_ARG0_GET(ctx);
    int irq;
    
    if (dest == 0)
    {
        int func = SERVICE_ARG1_GET(ctx);
        DEBUG("System service call from tid %d with func = %d\n", ut->tid, func);
        switch (func)
        {
        case SERVICE_SYS_LISTEN:
            ut->service_client = NULL;
            /* Listen for any request */
            semaphore_release(&ut->service_wait_sem);

            irq = __irq_save();
            while ((volatile user_thread_t)ut->service_client == NULL) {
                __irq_restore(irq);
                yield();
                irq = __irq_save();
            }
            service_context_transfer(ctx, ut->service_client->service_context);
            SERVICE_ARG0_SET(ctx, ut->service_client->tid);
            ut->service_client->service_context = NULL;
            __irq_restore(irq);
            
            break;
        case SERVICE_SYS_THREAD_CREATE:
            SERVICE_ARG1_SET(ctx, do_thread_create(SERVICE_ARG2_GET(ctx), SERVICE_ARG3_GET(ctx), SERVICE_ARG4_GET(ctx)));
            break;
        case SERVICE_SYS_THREAD_EXIT:
            do_thread_exit(SERVICE_ARG2_GET(ctx));
            break;
        }
    }
    else
    {
        int tid = SERVICE_ARG0_GET(ctx);
        user_thread_t target = user_thread_get_by_tid(tid);
        
        if (target == NULL)
        {
            /* XXX: more information */
            SERVICE_ARG0_SET(ctx, 0);
        }
        else
        {
            ut->service_context = ctx;
            semaphore_acquire(&target->service_wait_sem, NULL);
            target->service_client = ut;
            while ((volatile service_context_t)ut->service_context != NULL) yield();
            
            user_thread_put(target);
        }
    }
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
