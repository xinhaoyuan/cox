#include <iosem.h>
#include <arch/hashfn.h>
#include <irq.h>
#include <malloc.h>
#include <error.h>
#include <user.h>

void
iosem_hash_init(iosem_hash_t hash, size_t size, iosem_hash_head_t head_entry)
{
    hash->size = size;
    hash->head = head_entry;
    int i;

    for (i = 0; i < size; ++ i)
    {
        spinlock_init(&head_entry[i].lock);
        list_init(&head_entry[i].hash_list);
    }
}

static inline __attribute__((always_inline)) int
iosem_hash_node_get_locked(iosem_hash_t hash, uintptr_t key, int *irq_slot, iosem_hash_head_t *head_slot, iosem_hash_node_t *node_slot)
{
    uintptr_t h = hash_ptr(key) % hash->size;
    int irq = irq_save();
    spinlock_acquire(&hash->head[h].lock);

    iosem_hash_node_t node;
    list_entry_t head = &hash->head[h].hash_list;
    list_entry_t cur;
    
    if (list_empty(head))
    {
        node = NULL;
    }
    else
    {
        cur = list_next(head);
        while (1)
        {
            node = CONTAINER_OF(cur, iosem_hash_node_s, hash_node);
            if (node->key == key) break;

            if ((cur = list_next(cur)) == head)
            {
                node = NULL;
                break;
            }
        }
    }

    if (node == NULL)
    {
        node = kmalloc(sizeof(iosem_hash_node_s));

        if (node == NULL)
        {
            spinlock_release(&hash->head[h].lock);
            irq_restore(irq);

            return -E_NO_MEM;
        }
        
        node->key = key;
        node->up_count = 0;
        list_init(&node->down_queue);
        list_add_before(head, &node->hash_node);
    }

    *irq_slot  = irq;
    *node_slot = node;
    *head_slot = &hash->head[h];

    return 0;
}

int
iosem_down(iosem_hash_t hash, uintptr_t key, list_entry_t queue_node)
{
    iosem_hash_head_t head;
    iosem_hash_node_t node;
    int irq;
    int result = iosem_hash_node_get_locked(hash, key, &irq, &head, &node);
    if (result < 0) return result;
    
    result = node->up_count;
    
    if (result > 0)
        -- node->up_count;
    else
        list_add_before(&node->down_queue, queue_node);

    spinlock_release(&head->lock);
    irq_restore(irq);
        
    return result;
}

int
iosem_up(iosem_hash_t hash, uintptr_t key, size_t num)
{
    iosem_hash_head_t head;
    iosem_hash_node_t node;
    int irq;
    int result = iosem_hash_node_get_locked(hash, key, &irq, &head, &node);
    if (result < 0) return result;

    result = num;
    
    while (!list_empty(&node->down_queue) && num > 0)
    {
        list_entry_t cur = list_next(&node->down_queue);
        list_del(cur);
        -- num;

        io_ce_shadow_t shd = CONTAINER_OF(cur, io_ce_shadow_s, iosem_down_node);
        shd->type = IO_CE_SHADOW_TYPE_INIT;
        user_thread_iocb_push(shd->proc, shd->index);
    }

    result         -= num;
    node->up_count += num;

    spinlock_release(&head->lock);
    irq_restore(irq);

    return result;
}
