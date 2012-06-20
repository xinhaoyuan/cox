#include <mbox_irq.h>
#include <mbox.h>
#include <irq.h>
#include <lib/low_io.h>
#include <error.h>
#include <spinlock.h>
#include <user.h>

static struct mbox_irq_data_s
{
    int irq_no;
    spinlock_s listen_lock;
    list_entry_s listen_list;
} mbox_irq_data[IRQ_COUNT];

static volatile int mbox_irq_initialized = 0;

int
mbox_irq_init(void)
{
    int i;
    for (i = 0; i < IRQ_COUNT; ++ i)
    {
        mbox_irq_data[i].irq_no = i;
        spinlock_init(&mbox_irq_data[i].listen_lock);
        list_init(&mbox_irq_data[i].listen_list);
    }

    mbox_irq_initialized = 1;   
    return 0;
}

int
mbox_irq_listen(int irq_no, mbox_t mbox)
{
    if (!mbox_irq_initialized) return -E_INVAL;
    /* XXX irq_no check */
    
    /* Get for the link setting */
    mbox_get(mbox - mboxs); 

    int irq = irq_save();
    spinlock_acquire(&mbox->lock);
    
    if (mbox->status != MBOX_STATUS_NORMAL)
    {
        spinlock_release(&mbox->lock);
        irq_restore(irq);
        
        return -E_INVAL;
    }
    
    mbox->status = MBOX_STATUS_IRQ_LISTEN;
    spinlock_release(&mbox->lock);
    
    spinlock_acquire(&mbox_irq_data[irq_no].listen_lock);
    list_add_before(&mbox_irq_data[irq_no].listen_list, &mbox->irq_listen.listen_list);
    spinlock_release(&mbox_irq_data[irq_no].listen_lock);   
    irq_restore(irq);
    
    return 0;
}

static void
mbox_irq_send(mbox_send_io_t send_io, io_ce_shadow_t recv_shd, io_call_entry_t recv_ce)
{ }

static void
mbox_irq_ack(mbox_send_io_t send_io, io_ce_shadow_t recv_shd, io_call_entry_t recv_ce)
{ }

int
mbox_irq_handler(int irq_no, uint64_t acc)
{
    if (!mbox_irq_initialized) return -1;
    spinlock_acquire(&mbox_irq_data[irq_no].listen_lock);
    cprintf("IRQ %d -> MBOX\n", irq_no);
    list_entry_t l = list_next(&mbox_irq_data[irq_no].listen_list);
    while (l != &mbox_irq_data[irq_no].listen_list)
    {
        mbox_t mbox = CONTAINER_OF(l, mbox_s, irq_listen.listen_list);
        /* Hack code to detect whether the mbox is send-pending */
        spinlock_acquire(&mbox->io_lock);
        int to_send = list_empty(&mbox->send_io_list);
        spinlock_release(&mbox->io_lock);
        /* Not pending, send a mail without blocking*/
        if (to_send)
        {
            cprintf("IRQ MSG TO %d\n", mbox - mboxs);
            mbox_send_io_t io = mbox_send_io_acquire(1);
            if (io)
            {
                io->mbox         = mbox;
                io->send_cb      = mbox_irq_send;
                io->ack_cb       = mbox_irq_ack;
                io->iobuf_policy = 0;
                io->priv         = &mbox_irq_data[irq_no];                
                mbox_kern_send(io);
            }
        }
        l = list_next(l);
    }
    spinlock_release(&mbox_irq_data[irq_no].listen_lock);
    return 0;
}
