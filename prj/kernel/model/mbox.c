#include <mbox.h>
#include <irq.h>
#include <spinlock.h>
#include <error.h>
#include <string.h>
#include <page.h>
#include <user.h>
#include <proc.h>
#include <ips.h>

static list_entry_s mbox_free_list;
static spinlock_s   mbox_alloc_lock;

static list_entry_s mbox_send_io_free_list;
static spinlock_s   mbox_send_io_alloc_lock;
static semaphore_s  mbox_send_io_alloc_sem;

mbox_s mboxs[MBOXS_MAX_COUNT];

#define MBOX_SEND_IOS_MAX_COUNT 4096
static mbox_send_io_s mbox_send_ios[MBOX_SEND_IOS_MAX_COUNT];

mbox_t
mbox_get(int mbox_id)
{
    /* XXX: do ref stuff */
    return mboxs + mbox_id;
}

void
mbox_put(mbox_t mbox)
{
    /* XXX: do ref stuff */
}

int
mbox_init(void)
{
    list_init(&mbox_free_list);
    list_init(&mbox_send_io_free_list);

    spinlock_init(&mbox_alloc_lock);
    spinlock_init(&mbox_send_io_alloc_lock);
    semaphore_init(&mbox_send_io_alloc_sem, MBOX_SEND_IOS_MAX_COUNT);

    int i;
    for (i = 0; i != MBOXS_MAX_COUNT; ++ i)
    {
        mboxs[i].status = MBOX_STATUS_FREE;
        list_add_before(&mbox_free_list, &mboxs[i].free_list);
    }

    for (i = 0; i != MBOX_SEND_IOS_MAX_COUNT; ++ i)
    {
        mbox_send_ios[i].type = MBOX_SEND_IO_TYPE_FREE;
        list_add_before(&mbox_send_io_free_list, &mbox_send_ios[i].free_list);
    }

    return 0;
}


int
mbox_alloc(user_proc_t proc)
{
    list_entry_t l = NULL;
    
    int irq = irq_save();
    spinlock_acquire(&mbox_alloc_lock);

    if (!list_empty(&mbox_free_list))
    {
        l = list_next(&mbox_free_list);
        list_del(l);
    }

    spinlock_release(&mbox_alloc_lock);
    irq_restore(irq);

    if (l)
    {
        mbox_t mbox = CONTAINER_OF(l, mbox_s, free_list);

        spinlock_init(&mbox->lock);
        mbox->status = MBOX_STATUS_NORMAL;
        mbox->proc   = proc;

        list_init(&mbox->recv_io_list);
        list_init(&mbox->send_io_list);
        spinlock_init(&mbox->io_lock);

        /* XXX: do ref count stuff */

        return mbox - mboxs;
    }
    else return -1;
}

void
mbox_free(int mbox_id)
{
    mbox_put(mboxs + mbox_id);
}

mbox_send_io_t
mbox_send_io_acquire(mbox_t mbox, int try)
{
    if (try)
    {
        if (semaphore_try_acquire(&mbox_send_io_alloc_sem) == 0) return NULL;
    }
    else semaphore_acquire(&mbox_send_io_alloc_sem, NULL);
    
    int irq = irq_save();
    spinlock_acquire(&mbox_send_io_alloc_lock);
    list_entry_t l = list_next(&mbox_send_io_free_list);
    list_del(l);
    spinlock_release(&mbox_send_io_alloc_lock);
    irq_restore(irq);

    mbox_send_io_t io  = CONTAINER_OF(l, mbox_send_io_s, free_list);
    io->type      = MBOX_SEND_IO_TYPE_KERN;
    io->status    = MBOX_SEND_IO_STATUS_INIT;
    io->mbox      = mbox;
    
    return io;
}

int
mbox_send(mbox_send_io_t io, mbox_send_callback_f send_cb, void *send_data, mbox_ack_callback_f ack_cb, void *ack_data)
{
    mbox_t mbox = io->mbox;
    
    int irq = irq_save();
    spinlock_acquire(&mbox->io_lock);

    io->ack_cb    = ack_cb;
    io->ack_data  = ack_data;

    if (list_empty(&mbox->recv_io_list))
    {
        io->send_cb   = send_cb;
        io->send_data = send_data;
        io->status    = MBOX_SEND_IO_STATUS_QUEUEING;

        list_add_before(&mbox->send_io_list, &io->io_list);

        spinlock_release(&mbox->io_lock);
        irq_restore(irq);
        /* Get for the io send link */
        mbox_get(mbox - mboxs);
    }
    else
    {
        list_entry_t rl = list_next(&mbox->recv_io_list);
        list_del(rl);
        spinlock_release(&mbox->io_lock);
        irq_restore(irq);

        mbox_recv_io_t r = CONTAINER_OF(rl, mbox_recv_io_s, io_list);
        io_ce_shadow_t shd = CONTAINER_OF(r, io_ce_shadow_s, mbox_recv_io);
        
        io->status    = MBOX_SEND_IO_STATUS_PROCESSING;
        r->send_io    = io;

        io_call_entry_t ce = &shd->proc->user_thread->ioce.head[shd->index];
        /* WRITE MESSAGE */
        ce->ce.data[0] = 0;
        ce->ce.data[1] = r->iobuf_u;
        send_cb(send_data, r->iobuf, &ce->ce.data[2], &ce->ce.data[3]);
        user_thread_iocb_push(shd->proc, shd->index);
    }

    return 0;
}

int
mbox_io(mbox_t mbox, proc_t io_proc, iobuf_index_t io_index, uintptr_t ack_hint_a, uintptr_t ack_hint_b)
{
    int irq;

    io_ce_shadow_t shd = &io_proc->user_thread->ioce.shadows[io_index];
    mbox_send_io_t ack_io;
    
    if (shd->type == IO_CE_SHADOW_TYPE_MBOX_RECV_IO)
        ack_io = shd->mbox_recv_io.send_io;
    else if (shd->type == IO_CE_SHADOW_TYPE_INIT && mbox != NULL)
    {
        page_t p = page_alloc_atomic(MBOX_IOBUF_PSIZE);
        if (p == NULL)
            return -E_NO_MEM;

        shd->mbox_recv_io.iobuf = VADDR_DIRECT(PAGE_TO_PHYS(p));
        
        if (user_proc_arch_mmio_open(io_proc->user_proc, PADDR_DIRECT(shd->mbox_recv_io.iobuf),
                                     MBOX_IOBUF_PSIZE << __PGSHIFT, &shd->mbox_recv_io.iobuf_u))
        {
            page_free_atomic(p);
            return -E_NO_MEM;
        }

        shd->proc  = io_proc;
        shd->index = io_index;
        shd->type  = IO_CE_SHADOW_TYPE_MBOX_RECV_IO;
        shd->mbox_recv_io.mbox = mbox;
        ack_io = NULL;
    }
    else return -E_INVAL;

    if (ack_io)
    {
        ack_io->status = MBOX_SEND_IO_STATUS_FINISHED;
        ack_io->ack_cb(ack_io->ack_data, shd->mbox_recv_io.iobuf, ack_hint_a, ack_hint_b);

        if (ack_io->type == MBOX_SEND_IO_TYPE_KERN)
        {
            irq = irq_save();
            spinlock_acquire(&mbox_send_io_alloc_lock);
            list_add(&mbox_send_io_free_list, &ack_io->free_list);
            spinlock_release(&mbox_send_io_alloc_lock);
            irq_restore(irq);

            semaphore_release(&mbox_send_io_alloc_sem);
        }

        /* Put for the ack */
        mbox_put(ack_io->mbox);
    }

    if (mbox == NULL)
    {
        if (shd->type == IO_CE_SHADOW_TYPE_MBOX_RECV_IO)
        {
            page_free_atomic(PHYS_TO_PAGE(PADDR_DIRECT(shd->mbox_recv_io.iobuf)));
            user_proc_arch_mmio_close(io_proc->user_proc, shd->mbox_recv_io.iobuf_u);
            shd->type = IO_CE_SHADOW_TYPE_INIT;
        }
        return 0;
    }

    irq = irq_save();
    spinlock_acquire(&mbox->io_lock);
    
    if (list_empty(&mbox->send_io_list))
    {
        list_add_before(&mbox->recv_io_list, &shd->mbox_recv_io.io_list);
        spinlock_release(&mbox->io_lock);
        irq_restore(irq);
        /* Get for the recv link */
        mbox_get(mbox - mboxs);
    }
    else
    {
        list_entry_t l = list_next(&mbox->send_io_list);
        list_del(l);
        spinlock_release(&mbox->io_lock);
        irq_restore(irq);

        mbox_send_io_t io = CONTAINER_OF(l, mbox_send_io_s, io_list);
        io_call_entry_t ce = &io_proc->user_thread->ioce.head[io_index];
        io->status                = MBOX_SEND_IO_STATUS_PROCESSING;
        shd->mbox_recv_io.send_io = io;
        /* WRITE MESSAGE */
        ce->ce.data[0] = 0;
        ce->ce.data[1] =  shd->mbox_recv_io.iobuf_u;
        io->send_cb(io->send_data, shd->mbox_recv_io.iobuf, &ce->ce.data[2], &ce->ce.data[3]);
        user_thread_iocb_push(io_proc, io_index);
    }

    return 0;
}


void
mbox_send_func(void *__data, void *buf, uintptr_t *hint_a, uintptr_t *hint_b)
{
    mbox_io_data_t data = __data;
    if (data->send_buf != NULL)
    {
        *hint_a = data->send_buf_size;
        
        if (*hint_a > (MBOX_IOBUF_PSIZE << __PGSHIFT))
            *hint_a = (MBOX_IOBUF_PSIZE << __PGSHIFT);

        memmove(buf, data->send_buf, *hint_a);
    }
    else *hint_a = 0;
    *hint_b = data->hint_send;
}

void
mbox_ack_func(void *__data, void *buf, uintptr_t hint_a, uintptr_t hint_b)
{
    mbox_io_data_t data = __data;
    if (data->recv_buf != NULL);
    {
        if (hint_a > data->recv_buf_size)
            hint_a = data->recv_buf_size;
        if (hint_a > (MBOX_IOBUF_PSIZE << __PGSHIFT))
            hint_a = (MBOX_IOBUF_PSIZE << __PGSHIFT);

        memmove(data->recv_buf, buf, hint_a);
    }
    data->hint_ack = hint_b;

    IPS_NODE_WAIT_CLEAR(data->ips);
    proc_notify(IPS_NODE_PTR(data->ips));
}
