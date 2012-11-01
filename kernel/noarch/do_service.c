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
            ut->service_status = USER_THREAD_SERVICE_STATUS_LISTEN;
            semaphore_acquire(&ut->service_sem, NULL);
            while (ut->service_client == NULL) yield();
            service_context_transfer(ctx, ut->service_client->service_context);
            SERVICE_ARG0_SET(ctx, ut->service_client->tid);
            ut->service_client->service_context = NULL;
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
            goto err;


        irq = __irq_save();
        if (spinlock_try_acquire(&target->service_lock) == 0)
        {
            __irq_restore(irq);
            user_thread_put(target);
            goto err;
        }

        if (target->service_status != USER_THREAD_SERVICE_STATUS_LISTEN)
        {
            spinlock_release(&target->service_lock);
            __irq_restore(irq);
            user_thread_put(target);
            goto err;
        }
        target->service_status = USER_THREAD_SERVICE_STATUS_NO;
        
        ut->service_context = ctx;
        semaphore_release(&target->service_sem);
        target->service_client = ut;

        spinlock_release(&target->service_lock);
        __irq_restore(irq);
        
        while (ut->service_context != NULL) yield();
            
        user_thread_put(target);

      err:
        /* XXX: more information */
        SERVICE_ARG0_SET(ctx, 0);        
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
