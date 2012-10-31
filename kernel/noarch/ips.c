#define DEBUG_COMPONENT DBG_SCHED

#include <ips.h>
#include <proc.h>
#include <irq.h>

void
ips_wait_init(ips_node_t node, proc_t proc)
{
    IPS_NODE_WAIT_SET(node);
    IPS_NODE_AC_WAIT_SET(node);
    IPS_NODE_PTR_SET(node, proc);
}

void
ips_wait(ips_node_t node)
{
    while (1)
    {
        if (!IPS_NODE_WAIT(node) || !IPS_NODE_AC_WAIT(node))
            break;
        proc_wait_try();
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
mutex_init(mutex_t mutex)
{
    spinlock_init(&mutex->lock);
    MUTEX_HOLD_CLEAR(mutex);
}

int
mutex_try_acquire(mutex_t mutex)
{
    int result;
    int irq = irq_save();
    spinlock_acquire(&mutex->lock);
    result = !MUTEX_HOLD(mutex);
    if (result)
    {
        MUTEX_HOLD_SET(mutex);
        MUTEX_WAIT_CLEAR(mutex);
    }
    spinlock_release(&mutex->lock);
    irq_restore(irq);
     
    return result;
}

int
mutex_acquire(mutex_t mutex, ips_node_t node)
{
    if (node == NULL)
    {
        ips_node_s node;
        if (!mutex_acquire(mutex, &node))
        {
            ips_wait(&node);
            return 0;
        }
        else return 1;
    }
    else
    {
        int irq = irq_save();
        spinlock_acquire(&mutex->lock);
          
        if (!MUTEX_HOLD(mutex))
        {
            MUTEX_HOLD_SET(mutex);
            MUTEX_WAIT_CLEAR(mutex);
               
            spinlock_release(&mutex->lock);
            irq_restore(irq);
               
            IPS_NODE_WAIT_CLEAR(node);
            
            return 1;
        }
        else
        {
            ips_wait_init(node, current);
               
            if (MUTEX_WAIT(mutex))
            {
                node->next = MUTEX_PTR(mutex);
                node->prev = node->next->prev;
                node->next->prev = node;
                node->prev->next = node;
            }
            else
            {
                MUTEX_WAIT_SET(mutex);
                node->next = node->prev = node;
                MUTEX_PTR_SET(mutex, node);
            }
               
            spinlock_release(&mutex->lock);
            irq_restore(irq);

            return 0;
        }
    }
}

void
mutex_ac_break(mutex_t mutex, ips_node_t node)
{
    int irq = irq_save();
    spinlock_acquire(&mutex->lock);
    if (IPS_NODE_WAIT(node) && IPS_NODE_AC_WAIT(node))
    {
        node->next->prev = node->next;
        node->prev->next = node->prev;
          
        if (MUTEX_PTR(mutex) == node)
        {
            if (node->next == node)
                MUTEX_PTR_SET(mutex, NULL);
            else MUTEX_PTR_SET(mutex, node->next);
        }
    }
    spinlock_release(&mutex->lock);
    irq_restore(irq);
    
    IPS_NODE_AC_WAIT_CLEAR(node);
}

void
mutex_release(mutex_t mutex)
{
    proc_t notify = NULL;
    if (!MUTEX_HOLD(mutex)) return;
    
    int irq = irq_save();
    spinlock_acquire(&mutex->lock);
    if (MUTEX_WAIT(mutex))
    {
        ips_node_t node = MUTEX_PTR(mutex);
        IPS_NODE_WAIT_CLEAR(node);
          
        notify = IPS_NODE_PTR(node);
        node->next->prev = node->prev;
        node->prev->next = node->next;
          
        if (node->next == node)
            MUTEX_WAIT_CLEAR(mutex);
        else MUTEX_PTR_SET(mutex, node->next);
    }
    else MUTEX_HOLD_CLEAR(mutex);
    spinlock_release(&mutex->lock);
    irq_restore(irq);

    if (notify != NULL)
        proc_notify(notify);
}

void
semaphore_init(semaphore_t semaphore, uintptr_t count)
{
    spinlock_init(&semaphore->lock);
    semaphore->count = count;
    SEMAPHORE_WAIT_CLEAR(semaphore);
}

uintptr_t
semaphore_try_acquire(semaphore_t semaphore)
{
    uintptr_t result;
    int irq = irq_save();
    spinlock_acquire(&semaphore->lock);
    result = semaphore->count;
    if (result > 0)
    {
        if (-- semaphore->count == 0)
            SEMAPHORE_WAIT_CLEAR(semaphore);
    }
    spinlock_release(&semaphore->lock);
    irq_restore(irq);
    return result;
}

uintptr_t
semaphore_acquire(semaphore_t semaphore, ips_node_t node)
{
    uintptr_t result;
    if (node == NULL)
    {
        ips_node_s node;
        result = semaphore_acquire(semaphore, &node);
        if (!result)
            ips_wait(&node);

        return result;
    }
    else
    {
        int irq = irq_save();
        spinlock_acquire(&semaphore->lock);
        result = semaphore->count;
        if (result > 0)
        {
            if (-- semaphore->count == 0)
                SEMAPHORE_WAIT_CLEAR(semaphore);
          
            spinlock_release(&semaphore->lock);
            irq_restore(irq);
               
            IPS_NODE_WAIT_CLEAR(node);
          
            return result;
        }
        else
        {
            ips_wait_init(node, current);

            if (SEMAPHORE_WAIT(semaphore))
            {
                node->next = SEMAPHORE_PTR(semaphore);
                node->prev = node->next->prev;
                node->next->prev = node;
                node->prev->next = node;
            }
            else
            {
                SEMAPHORE_WAIT_SET(semaphore);
                node->next = node->prev = node;
                SEMAPHORE_PTR_SET(semaphore, node);
            }

            spinlock_release(&semaphore->lock);
            irq_restore(irq);
               
            return result;
        }         
    }
}

void
semaphore_ac_break(semaphore_t semaphore, ips_node_t node)
{
    int irq = irq_save();
    spinlock_acquire(&semaphore->lock);
    if (IPS_NODE_WAIT(node) && IPS_NODE_AC_WAIT(node))
    {
        node->next->prev = node->next;
        node->prev->next = node->prev;
        if (SEMAPHORE_PTR(semaphore) == node)
        {
            if (node->next == node)
                SEMAPHORE_WAIT_CLEAR(semaphore);
            else SEMAPHORE_PTR_SET(semaphore, node->next);
        }
    }
    spinlock_release(&semaphore->lock);
    irq_restore(irq);

    IPS_NODE_AC_WAIT_CLEAR(node);
}

#include <user.h>

int
semaphore_release(semaphore_t semaphore)
{
    int result = 0;
    proc_t notify = NULL;
    int irq = irq_save();
    spinlock_acquire(&semaphore->lock);
    if (SEMAPHORE_WAIT(semaphore))
    {
        ips_node_t node = SEMAPHORE_PTR(semaphore);
        IPS_NODE_WAIT_CLEAR(node);

        notify = IPS_NODE_PTR(node);

        node->next->prev = node->prev;
        node->prev->next = node->next;
        if (node->next == node)
            SEMAPHORE_WAIT_CLEAR(semaphore);
        else SEMAPHORE_PTR_SET(semaphore, node->next);

        result = 1;
    }
    else
    {
        ++ semaphore->count;
    }
    spinlock_release(&semaphore->lock);
    irq_restore(irq);

    if (notify != NULL)
        proc_notify(notify);

    return result;
}
