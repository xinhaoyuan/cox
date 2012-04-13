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
            
            io_call_entry_t ce = &USER_THREAD(shd->proc).ioce[shd->index];
            /* WRITE MESSAGE */
            ce->data[0] = 0;
            io->send_cb(io, shd, ce);
            user_thread_iocb_push(shd->proc, shd->index);

            return 0;
        }
        
        default:
            return -E_INVAL;
        }
    }
}

int
mbox_user_send(proc_t io_proc, iobuf_index_t io_index)
{
    int irq;
    io_ce_shadow_t send_shd = &USER_THREAD(io_proc).ioce_shadow[io_index];
    io_call_entry_t send_ce = &USER_THREAD(io_proc).ioce[io_index];
    mbox_send_io_t  send_io = &send_shd->mbox_send_io;
    mbox_t             mbox = send_io->mbox;

    if (send_shd->type != IO_CE_SHADOW_TYPE_MBOX_SEND_IO)
    {
        send_ce->data[0] = -E_INVAL;
        return 0;
    }

    send_io->iobuf_policy = send_ce->data[1];
    if ((send_io->iobuf_policy & MBOX_IOBUF_POLICY_DO_MAP) && send_io->iobuf_target_u == 0)
    {
        if (user_proc_arch_mmio_open(mbox->proc, PAGE_TO_PHYS(send_io->iobuf_p),
                                     send_io->iobuf_size, &send_io->iobuf_target_u))
        {
            send_ce->data[0] = -E_NO_MEM;
            return 0;
        }
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

        mbox_recv_io_t  recv_io = CONTAINER_OF(rl, mbox_recv_io_s, io_list);
        io_ce_shadow_t recv_shd = CONTAINER_OF(recv_io, io_ce_shadow_s, mbox_recv_io);
        io_call_entry_t recv_ce = &USER_THREAD(recv_shd->proc).ioce[recv_shd->index];
        
        send_io->status  = MBOX_SEND_IO_STATUS_PROCESSING;
        recv_io->send_io = send_io;

        if (send_io->iobuf_policy)
        {
            recv_io->iobuf_u = send_io->iobuf_target_u;
            recv_ce->data[0] = 0;
            recv_ce->data[1] = recv_io->iobuf_u;
        }
        else
        {
            recv_ce->data[0] = 0;
            recv_ce->data[1] = 0;
        }
        
        memmove(&recv_ce->data[2], &send_ce->data[2], sizeof(uintptr_t) * (IO_ARGS_COUNT - 2));
        user_thread_iocb_push(recv_shd->proc, recv_shd->index);
        
        return 1;
    }
}

static void
mbox_ack_io(mbox_send_io_t ack_io, io_ce_shadow_t recv_shd, io_call_entry_t recv_ce)
{
    mbox_recv_io_t recv_io = &recv_shd->mbox_recv_io;
    int irq;

    if ((ack_io->iobuf_policy & MBOX_IOBUF_POLICY_DO_MAP) &&
        !(ack_io->iobuf_policy & MBOX_IOBUF_POLICY_PERSISTENT))
    {
        user_proc_arch_mmio_close(USER_THREAD(recv_shd->proc).user_proc, recv_io->iobuf_u);
        ack_io->iobuf_target_u = recv_io->iobuf_u = 0;
    }
    
    switch (ack_io->type)
    {
    case MBOX_SEND_IO_TYPE_KERN_STATIC:
        ack_io->ack_cb(ack_io, recv_shd, recv_ce);
        break;

    case MBOX_SEND_IO_TYPE_KERN_ALLOC:
        ack_io->status = MBOX_SEND_IO_STATUS_FINISHED;
        ack_io->ack_cb(ack_io, recv_shd, recv_ce);

        irq = irq_save();
        spinlock_acquire(&mbox_send_io_alloc_lock);
        list_add(&mbox_send_io_free_list, &ack_io->free_list);
        spinlock_release(&mbox_send_io_alloc_lock);
        irq_restore(irq);

        semaphore_release(&mbox_send_io_alloc_sem);
        mbox_put(ack_io->mbox);
        break;

    case MBOX_SEND_IO_TYPE_USER_SHADOW:
        ack_io->status = MBOX_SEND_IO_STATUS_FINISHED;
        
        io_ce_shadow_t  ack_shd = CONTAINER_OF(ack_io, io_ce_shadow_s, mbox_send_io);
        io_call_entry_t ack_ce  = &USER_THREAD(ack_shd->proc).ioce[ack_shd->index];
        
        ack_ce->data[0] = 0;
        ack_ce->data[1] = ack_io->iobuf_u;
        memmove(&ack_ce->data[2], &recv_ce->data[2], sizeof(uintptr_t) * (IO_ARGS_COUNT - 2));
        user_thread_iocb_push(ack_shd->proc, ack_shd->index);

        mbox_put(ack_io->mbox);
        break;

    }
}

int
mbox_user_recv(proc_t io_proc, iobuf_index_t io_index)
{
    int irq;

    io_ce_shadow_t recv_shd = &USER_THREAD(io_proc).ioce_shadow[io_index];
    io_call_entry_t recv_ce = &USER_THREAD(io_proc).ioce[io_index];
    mbox_recv_io_t  recv_io = &recv_shd->mbox_recv_io;
    mbox_t             mbox = recv_io->mbox;
    
    if (recv_shd->type != IO_CE_SHADOW_TYPE_MBOX_RECV_IO)
    {
        recv_ce->data[0] = -E_INVAL;
        return 0;
    }

    if (recv_io->send_io) mbox_ack_io(recv_io->send_io, recv_shd, recv_ce);
    recv_io->send_io = NULL;

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
            io_ce_shadow_t send_shd = CONTAINER_OF(send_io, io_ce_shadow_s, mbox_send_io);
            io_call_entry_t send_ce = &USER_THREAD(send_shd->proc).ioce[send_shd->index];

            if (send_io->iobuf_policy)
            {
                /* XXX: indicate the page was mapped */
                recv_io->iobuf_u = send_io->iobuf_target_u;
                recv_ce->data[0] = 0;
                recv_ce->data[1] = recv_io->iobuf_u;
            }
            else
            {
                recv_ce->data[0] = 0;
                recv_ce->data[1] = 0;
            }

            memmove(&recv_ce->data[2], &send_ce->data[2], sizeof(uintptr_t) * (IO_ARGS_COUNT - 2));
        }
        else
        {
            send_io->send_cb(send_io, recv_shd, recv_ce);
        }

        recv_io->send_io = send_io;
        user_thread_iocb_push(io_proc, io_index);
    }

    return 1;
}

int
mbox_user_io_attach(proc_t io_proc, iobuf_index_t io_index, mbox_t mbox, int type, size_t buf_size)
{
    int err;
    io_ce_shadow_t shd = &USER_THREAD(io_proc).ioce_shadow[io_index];
    io_call_entry_t ce = &USER_THREAD(io_proc).ioce[io_index];
    
    buf_size = (buf_size + __PGSIZE - 1) & ~(__PGSIZE - 1);
    
    if (type != IO_CE_SHADOW_TYPE_MBOX_RECV_IO &&
        type != IO_CE_SHADOW_TYPE_MBOX_SEND_IO) return -E_INVAL;
    
    if (shd->type != IO_CE_SHADOW_TYPE_INIT &&
        shd->type != type)
        return -E_INVAL;

    if (type == IO_CE_SHADOW_TYPE_MBOX_RECV_IO)
    {
        mbox_recv_io_t recv_io = &shd->mbox_recv_io;
        if (shd->type == type && recv_io->send_io != NULL)
            return -E_BUSY;

        if (shd->type == IO_CE_SHADOW_TYPE_INIT)
        {
            shd->proc  = io_proc;
            shd->index = io_index;
            shd->type  = type;
        }

        recv_io->send_io = NULL;
        recv_io->mbox = mbox;
    }
    else
    {
        mbox_send_io_t send_io = &shd->mbox_send_io;
        if (shd->type == type)
        {
            if (send_io->iobuf_target_u)
                user_proc_arch_mmio_close(send_io->mbox->proc, send_io->iobuf_target_u);
            user_proc_arch_mmio_close(USER_THREAD(io_proc).user_proc, send_io->iobuf_u);
            page_free_atomic(send_io->iobuf_p);
        }
        else
        {
            shd->proc  = io_proc;
            shd->index = io_index;
            shd->type  = type;
        }

        if ((send_io->iobuf_p = page_alloc_atomic(buf_size)) == NULL)
        {
            err = -E_NO_MEM;
            goto error;
        }
        
        if (user_proc_arch_mmio_open(USER_THREAD(io_proc).user_proc, PAGE_TO_PHYS(send_io->iobuf_p),
                                     buf_size, &send_io->iobuf_u))
        {
            page_free_atomic(send_io->iobuf_p);
            err = -E_NO_MEM;
            goto error;
        }

        send_io->type = MBOX_SEND_IO_TYPE_USER_SHADOW;
        
        send_io->iobuf_size     = buf_size;
        send_io->iobuf_policy   = 0;
        send_io->iobuf_target_u = 0;

        send_io->mbox = mbox;

        ce->data[1] = send_io->iobuf_u;
    }

    return 0;

  error:

    shd->type = IO_CE_SHADOW_TYPE_INIT;
    return err;
}

int
mbox_user_io_detach(proc_t io_proc, iobuf_index_t io_index)
{
    io_ce_shadow_t shd = &USER_THREAD(io_proc).ioce_shadow[io_index];
    if (shd->type == IO_CE_SHADOW_TYPE_MBOX_RECV_IO)
    {
        mbox_recv_io_t recv_io = &shd->mbox_recv_io;
        if (recv_io->send_io) mbox_ack_io(recv_io->send_io, shd, USER_THREAD(io_proc).ioce + io_index);
    }
    else if (shd->type == IO_CE_SHADOW_TYPE_MBOX_SEND_IO)
    {
        mbox_send_io_t send_io = &shd->mbox_send_io;
        user_proc_arch_mmio_close(USER_THREAD(io_proc).user_proc, send_io->iobuf_u);
        page_free_atomic(send_io->iobuf_p);
        /* XXX kernel map of iobuf? */
    }
    else return -E_INVAL;

    shd->type = IO_CE_SHADOW_TYPE_INIT;
    return 0;
}
