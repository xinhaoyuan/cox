#define DEBUG_COMPONENT DBG_IO

#include <debug.h>
#include <service.h>
#include <do_service.h>
#include <user.h>
#include <ips.h>

static int  do_thread_create(uintptr_t entry, uintptr_t tls, uintptr_t stack);
static void do_thread_exit(int code);

void
do_service(service_context_t ctx)
{
    user_thread_t ut = USER_THREAD(current);
    uintptr_t dest = SERVICE_ARG0_GET(ctx);

    if (ut->service_source != NULL)
    {
        /* Reply */
        
        SERVICE_ARG0_SET(ctx, ut->tid);
        
        service_context_transfer(ut->service_source->service_context, ctx);
        semaphore_release(&ut->service_source->service_fill_sem);

        ut->service_source = NULL;
        return;
    }

    if (dest == 0)
    {
        int func = SERVICE_ARG1_GET(ctx);
        DEBUG("System service cal with func = %d\n", func);
        switch (func)
        {
        case SERVICE_SYS_LISTEN:
            /* Listen for any request */
            semaphore_release(&ut->service_wait_sem);
            semaphore_acquire(&ut->service_fill_sem, NULL);
            service_context_transfer(ctx, ut->service_source->service_context);
            break;
        case SERVICE_SYS_THREAD_CREATE:
            SERVICE_ARG1_SET(ctx, do_thread_create(SERVICE_ARG2_GET(ctx), SERVICE_ARG3_GET(ctx), SERVICE_ARG4_GET(ctx)));
            break;
        case SERVICE_SYS_THREAD_EXIT:
            do_thread_exit(SERVICE_ARG2_GET(ctx));
        }
    }
    else
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
    }
}

int 
do_thread_create(uintptr_t entry, uintptr_t tls, uintptr_t stack)
{
    user_thread_t ut = USER_THREAD(current);
    return user_thread_create_from_thread(ut->proc.name, ut, entry, tls, stack) == NULL;
}

void
do_thread_exit(int code)
{
    /* XXX */
}
