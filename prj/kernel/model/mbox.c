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
mbox_sys_init(void)
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
mbox_send_io_acquire(int try)
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
    io->type      = MBOX_SEND_IO_TYPE_KERN_ALLOC;
    io->status    = MBOX_SEND_IO_STATUS_INIT;
    
    return io;
}

int
mbox_kern_send(mbox_send_io_t io)
{
    mbox_t mbox = io->mbox;

    if (io->type == MBOX_SEND_IO_TYPE_USER_SHADOW)
        return -E_INVAL;
    
    
    int irq = irq_save();
    spinlock_acquire(&mbox->io_lock);

    if (list_empty(&mbox->recv_io_list))
    {
        io->status    = MBOX_SEND_IO_STATUS_QUEUEING;
        list_add_before(&mbox->send_io_list, &io->io_list);
        spinlock_release(&mbox->io_lock);
        irq_restore(irq);
        /* Get for the io send link */
        mbox_get(mbox - mboxs);

        return 0;
    }
    else
    {
        switch (io->type)
        {
        case MBOX_SEND_IO_TYPE_KERN_STATIC:
            spinlock_release(&mbox->io_lock);
            irq_restore(irq);

            return -E_BUSY;

        case MBOX_SEND_IO_TYPE_KERN_ALLOC:
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
            io->send_cb(io->send_data, r->iobuf, &ce->ce.data[2], &ce->ce.data[3]);
            user_thread_iocb_push(shd->proc, shd->index);

            return 0;
        }
        
        default:
            return -E_INVAL;
        }
    }
}

int
mbox_user_send(mbox_t mbox, proc_t io_proc, iobuf_index_t io_index, uintptr_t hint_a, uintptr_t hint_b)
{
    int irq;
    io_ce_shadow_t send_shd = &io_proc->user_thread->ioce.shadows[io_index];
    io_call_entry_t send_ce = &io_proc->user_thread->ioce.head[io_index];
    mbox_send_io_t  send_io = &send_shd->mbox_send_io;

    if (send_shd->type == IO_CE_SHADOW_TYPE_MBOX_SEND_IO && mbox != NULL)
    {
        send_io->mbox = mbox;
        send_io->hint_a = hint_a;
        send_io->hint_b = hint_b;

        send_ce->ce.data[0] = 0;
        send_ce->ce.data[1] = send_io->iobuf_u;
    }
    else if (send_shd->type == IO_CE_SHADOW_TYPE_INIT)
    {
        send_io->type    = MBOX_SEND_IO_TYPE_USER_SHADOW;
        send_io->iobuf_p = page_alloc_atomic(MBOX_IOBUF_PSIZE);
        if (send_io->iobuf_p == NULL)
        {
            send_ce->ce.data[0] = -E_NO_MEM;
            return 0;
        }

        if (user_proc_arch_mmio_open(io_proc->user_proc, PAGE_TO_PHYS(send_io->iobuf_p),
                                     MBOX_IOBUF_PSIZE << __PGSHIFT, &send_io->iobuf_u))
        {
            page_free_atomic(send_io->iobuf_p);
            send_ce->ce.data[0] = -E_NO_MEM;
            return 0;
        }

        send_shd->proc  = io_proc;
        send_shd->index = io_index;
        send_shd->type  = IO_CE_SHADOW_TYPE_MBOX_SEND_IO;

        send_ce->ce.data[0] = 0;
        send_ce->ce.data[1] = send_io->iobuf_u;
        return 0;
    }
    else
    {
        send_ce->ce.data[0] = -E_INVAL;
        return 0;
    }

    irq = irq_save();
    spinlock_acquire(&mbox->io_lock);

    if (list_empty(&mbox->recv_io_list))
    {
        send_io->status = MBOX_SEND_IO_STATUS_QUEUEING;
        list_add_before(&mbox->send_io_list, &send_io->io_list);
        spinlock_release(&mbox->io_lock);
        irq_restore(irq);
        /* Get for the io send link */
        mbox_get(mbox - mboxs);

        return 1;
    }
    else
    {
        list_entry_t rl = list_next(&mbox->recv_io_list);
        list_del(rl);
        spinlock_release(&mbox->io_lock);
        irq_restore(irq);
            
        mbox_recv_io_t recv_io = CONTAINER_OF(rl, mbox_recv_io_s, io_list);
        io_ce_shadow_t recv_shd = CONTAINER_OF(recv_io, io_ce_shadow_s, mbox_recv_io);
        
        send_io->status  = MBOX_SEND_IO_STATUS_PROCESSING;
        recv_io->send_io = send_io;

        /* XXX: PROCESS ERROR HERE */
        user_proc_arch_mmio_open(recv_shd->proc->user_proc, PAGE_TO_PHYS(send_io->iobuf_p),
                                 MBOX_IOBUF_PSIZE << __PGSHIFT, &recv_io->iobuf_u_target);
        
        io_call_entry_t recv_ce = &recv_shd->proc->user_thread->ioce.head[recv_shd->index];
        recv_ce->ce.data[0] = 0;
        recv_ce->ce.data[1] = recv_io->iobuf_u_target;
        recv_ce->ce.data[2] = send_io->hint_a;
        recv_ce->ce.data[3] = send_io->hint_b;
        user_thread_iocb_push(recv_shd->proc, recv_shd->index);
        
        return 1;
    }
}

int
mbox_user_recv(mbox_t mbox, proc_t io_proc, iobuf_index_t io_index, uintptr_t ack_hint_a, uintptr_t ack_hint_b)
{
    int irq;

    io_ce_shadow_t recv_shd = &io_proc->user_thread->ioce.shadows[io_index];
    io_call_entry_t recv_ce = &io_proc->user_thread->ioce.head[io_index];
    mbox_recv_io_t recv_io  = &recv_shd->mbox_recv_io;
    mbox_send_io_t ack_io;
    
    if (recv_shd->type == IO_CE_SHADOW_TYPE_MBOX_RECV_IO)
        ack_io = recv_io->send_io;
    else if (recv_shd->type == IO_CE_SHADOW_TYPE_INIT && mbox != NULL)
    {
        recv_io->iobuf_p = page_alloc_atomic(MBOX_IOBUF_PSIZE);
        if (recv_io->iobuf_p == NULL)
        {
            recv_ce->ce.data[0] = -E_NO_MEM;
            return 0;
        }

        recv_io->iobuf = VADDR_DIRECT(PAGE_TO_PHYS(recv_io->iobuf_p));
        
        if (user_proc_arch_mmio_open(io_proc->user_proc, PAGE_TO_PHYS(recv_io->iobuf_p),
                                     MBOX_IOBUF_PSIZE << __PGSHIFT, &recv_io->iobuf_u))
        {
            page_free_atomic(recv_io->iobuf_p);
            recv_ce->ce.data[0] = -E_NO_MEM;
            return 0;
        }

        recv_io->iobuf_u_target = recv_io->iobuf_u;
        recv_io->mbox = mbox;
        
        recv_shd->proc  = io_proc;
        recv_shd->index = io_index;
        recv_shd->type  = IO_CE_SHADOW_TYPE_MBOX_RECV_IO;
        ack_io = NULL;
    }
    else return -E_INVAL;

    if (ack_io)
    {
        switch (ack_io->type)
        {
        case MBOX_SEND_IO_TYPE_KERN_ALLOC:
            ack_io->status = MBOX_SEND_IO_STATUS_FINISHED;
            ack_io->ack_cb(ack_io->ack_data, recv_io->iobuf, ack_hint_a, ack_hint_b);

            irq = irq_save();
            spinlock_acquire(&mbox_send_io_alloc_lock);
            list_add(&mbox_send_io_free_list, &ack_io->free_list);
            spinlock_release(&mbox_send_io_alloc_lock);
            irq_restore(irq);

            semaphore_release(&mbox_send_io_alloc_sem);
            mbox_put(ack_io->mbox);
            break;

        case MBOX_SEND_IO_TYPE_USER_SHADOW:
            if (recv_io->iobuf_u_target != recv_io->iobuf_u)
                user_proc_arch_mmio_close(io_proc->user_proc, recv_io->iobuf_u_target);
            
            ack_io->status = MBOX_SEND_IO_STATUS_FINISHED;
            io_ce_shadow_t ack_shd = CONTAINER_OF(ack_io, io_ce_shadow_s, mbox_send_io);
            user_thread_iocb_push(ack_shd->proc, ack_shd->index);
            
            mbox_put(ack_io->mbox);
            break;

        case MBOX_SEND_IO_TYPE_KERN_STATIC:
            ack_io->ack_cb(ack_io->ack_data, recv_io->iobuf, ack_hint_a, ack_hint_b);

            break;
        }
    }

    if (mbox == NULL)
    {
        recv_io->send_io = NULL;
        recv_ce->ce.data[0] = 0;
        return 0;
    }

    irq = irq_save();
    spinlock_acquire(&mbox->io_lock);
    
    if (list_empty(&mbox->send_io_list))
    {
        list_add_before(&mbox->recv_io_list, &recv_io->io_list);
        spinlock_release(&mbox->io_lock);
        irq_restore(irq);
        /* Get for the recv link */
        mbox_get(mbox - mboxs);
    }
    else
    {
        list_entry_t l = list_next(&mbox->send_io_list);
        mbox_send_io_t send_io = CONTAINER_OF(l, mbox_send_io_s, io_list);
        if (send_io->type != MBOX_SEND_IO_TYPE_KERN_STATIC)
        {
            list_del(l);
            send_io->status = MBOX_SEND_IO_STATUS_PROCESSING;
        }
        spinlock_release(&mbox->io_lock);
        irq_restore(irq);

        

        if (send_io->type == MBOX_SEND_IO_TYPE_USER_SHADOW)
        {
            /* XXX: PROCESS ERROR HERE */
            user_proc_arch_mmio_open(io_proc->user_proc, PAGE_TO_PHYS(send_io->iobuf_p),
                                     MBOX_IOBUF_PSIZE << __PGSHIFT, &recv_io->iobuf_u_target);

            recv_ce->ce.data[2] = send_io->hint_a;
            recv_ce->ce.data[3] = send_io->hint_b;
        }
        else
        {
            recv_io->iobuf_u_target = recv_io->iobuf_u;
            send_io->send_cb(send_io->send_data, recv_io->iobuf, &recv_ce->ce.data[2], &recv_ce->ce.data[3]);
        }

        
        recv_io->send_io = send_io;
        recv_ce->ce.data[0] = 0;
        recv_ce->ce.data[1] = recv_io->iobuf_u_target;
        user_thread_iocb_push(io_proc, io_index);
    }

    return 1;
}

int
mbox_user_io_end(proc_t io_proc, iobuf_index_t io_index)
{
    io_ce_shadow_t shd = &io_proc->user_thread->ioce.shadows[io_index];
    if (shd->type == IO_CE_SHADOW_TYPE_MBOX_RECV_IO)
    {
        mbox_recv_io_t recv_io = &shd->mbox_recv_io;
        user_proc_arch_mmio_close(io_proc->user_proc, recv_io->iobuf_u);
        page_free_atomic(recv_io->iobuf);
        /* XXX kernel map of iobuf? */

        shd->type = IO_CE_SHADOW_TYPE_INIT;
    }
    else if (shd->type == IO_CE_SHADOW_TYPE_MBOX_SEND_IO)
    {
        mbox_recv_io_t send_io = &shd->mbox_send_io;
        user_proc_arch_mmio_close(io_proc->user_proc, send_io->iobuf_u);
        page_free_atomic(send_io->iobuf);
        /* XXX kernel map of iobuf? */

        shd->type = IO_CE_SHADOW_TYPE_INIT;
    }
    else return -E_INVAL;
    
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
