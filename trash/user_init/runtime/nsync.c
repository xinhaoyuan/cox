#include <runtime/nsync.h>
#include <runtime/fiber.h>
#include <runtime/local.h>

void
ips_wait(ips_node_t node)
{
    while (1)
    {
        if (!IPS_NODE_WAIT(node) || !IPS_NODE_AC_WAIT(node))
            break;
        fiber_wait_try();
    }
}

int
ips_wait_try(ips_node_t node)
{
    return !IPS_NODE_WAIT(node) || !IPS_NODE_AC_WAIT(node);
}

int
ips_is_finish(ips_node_t node)
{
    return !IPS_NODE_WAIT(node);
}

void
native_sem_init(native_sem_t native_sem, uintptr_t count)
{
    spinlock_init(&native_sem->lock);
    native_sem->count = count;
    NATIVE_SEM_WAIT_CLEAR(native_sem);
}

uintptr_t
native_sem_try_acquire(native_sem_t native_sem)
{
    uintptr_t result;
    spinlock_acquire(&native_sem->lock);
    result = native_sem->count;
    if (result > 0)
    {
        if (-- native_sem->count == 0)
            NATIVE_SEM_WAIT_CLEAR(native_sem);
    }
    spinlock_release(&native_sem->lock);
    return result;
}

uintptr_t
native_sem_acquire(native_sem_t native_sem, ips_node_t node)
{
    uintptr_t result;
    if (node == NULL)
    {
        ips_node_s node;
        result = native_sem_acquire(native_sem, &node);
        if (!result)
            ips_wait(&node);

        return result;
    }
    else
    {
        spinlock_acquire(&native_sem->lock);
        result = native_sem->count;
        if (result > 0)
        {
            if (-- native_sem->count == 0)
                NATIVE_SEM_WAIT_CLEAR(native_sem);
          
            spinlock_release(&native_sem->lock);
               
            IPS_NODE_WAIT_CLEAR(node);
          
            return result;
        }
        else
        {
            IPS_NODE_PTR_SET(node, __current_fiber);
            IPS_NODE_AC_WAIT_SET(node);
            IPS_NODE_WAIT_SET(node);

            if (NATIVE_SEM_WAIT(native_sem))
            {
                node->next = NATIVE_SEM_PTR(native_sem);
                node->prev = node->next->prev;
                node->next->prev = node;
                node->prev->next = node;
            }
            else
            {
                NATIVE_SEM_WAIT_SET(native_sem);
                node->next = node->prev = node;
                NATIVE_SEM_PTR_SET(native_sem, node);
            }

            spinlock_release(&native_sem->lock);
               
            return result;
        }         
    }
}

void
native_sem_ac_break(native_sem_t native_sem, ips_node_t node)
{
    spinlock_acquire(&native_sem->lock);
    if (IPS_NODE_WAIT(node) && IPS_NODE_AC_WAIT(node))
    {
        node->next->prev = node->next;
        node->prev->next = node->prev;
        if (NATIVE_SEM_PTR(native_sem) == node)
        {
            if (node->next == node)
                NATIVE_SEM_WAIT_CLEAR(native_sem);
            else NATIVE_SEM_PTR_SET(native_sem, node->next);
        }
    }
    spinlock_release(&native_sem->lock);

    IPS_NODE_AC_WAIT_CLEAR(node);
}

int
native_sem_release(native_sem_t native_sem)
{
    int result = 0;
    fiber_t notify = NULL;
    spinlock_acquire(&native_sem->lock);
    if (NATIVE_SEM_WAIT(native_sem))
    {
        ips_node_t node = NATIVE_SEM_PTR(native_sem);
        IPS_NODE_WAIT_CLEAR(node);
          
        notify = IPS_NODE_PTR(node);
        node->next->prev = node->prev;
        node->prev->next = node->next;
        if (node->next == node)
            NATIVE_SEM_WAIT_CLEAR(native_sem);
        else NATIVE_SEM_PTR_SET(native_sem, node->next);

        result = 1;
    }
    else
    {
        ++ native_sem->count;
    }
    spinlock_release(&native_sem->lock);

    if (notify != NULL)
        fiber_notify(notify);

    return result;
}
