#define DEBUG_COMPONENT DBG_IO

#include <debug.h>
#include <service.h>
#include <do_service.h>
#include <user.h>
#include <ips.h>

void
do_service(service_context_t ctx)
{
    DEBUG("service\n");
    return;
    
    user_thread_t ut = USER_THREAD(current);
    uintptr_t dest = SERVICE_ARG0_GET(ctx);

    if (ut->service_source != NULL)
    {
        /* Reply */
        
        /* XXX */
        SERVICE_ARG0_SET(ctx, ut->tid);
        
        service_context_transfer(ut->service_source->service_context, ctx);
        semaphore_release(&ut->service_source->service_fill_sem);

        ut->service_source = NULL;
        return;
    }

    switch (dest)
    {
    case 0:
        /* Listen for any request */
        semaphore_release(&ut->service_wait_sem);
        semaphore_acquire(&ut->service_fill_sem, NULL);
        service_context_transfer(ctx, ut->service_source->service_context);
        break;

    default:
    {
        user_thread_t target = user_thread_get_by_tid(SERVICE_ARG0_GET(ctx));
        if (target == NULL)
        {
            /* XXX failed */
            SERVICE_ARG0_SET(ctx, 0);
        }
        else
        {
            semaphore_acquire(&target->service_wait_sem, NULL);
            ut->service_context = ctx;
            target->service_source = ut;
            semaphore_release(&target->service_fill_sem);
            /* Wait for sync reply */
            semaphore_acquire(&ut->service_fill_sem, NULL);
        }
        user_thread_put(target);
        break;
    }
    }
}
