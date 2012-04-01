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

static list_entry_s mbox_io_free_list;
static spinlock_s   mbox_io_alloc_lock;
static semaphore_s  mbox_io_alloc_sem;

mbox_s mboxs[MBOXS_MAX_COUNT];
mbox_io_s mbox_ios[MBOX_IOS_MAX_COUNT];

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
	list_init(&mbox_io_free_list);

	spinlock_init(&mbox_alloc_lock);
	spinlock_init(&mbox_io_alloc_lock);
	semaphore_init(&mbox_io_alloc_sem, MBOX_IOS_MAX_COUNT);

	int i;
	for (i = 0; i != MBOXS_MAX_COUNT; ++ i)
	{
		mboxs[i].status = MBOX_STATUS_FREE;
		list_add_before(&mbox_free_list, &mboxs[i].free_list);
	}

	for (i = 0; i != MBOX_IOS_MAX_COUNT; ++ i)
	{
		mbox_ios[i].status = MBOX_IO_STATUS_FREE;
		list_add_before(&mbox_io_free_list, &mbox_ios[i].free_list);
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

		list_init(&mbox->io_recv_list);
		list_init(&mbox->io_send_list);
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

int
mbox_send(mbox_t mbox, int try, mbox_send_callback_f send_cb, void *send_data, mbox_ack_callback_f ack_cb, void *ack_data)
{
	if (try)
	{
		if (semaphore_try_acquire(&mbox_io_alloc_sem) == 0) return -E_NO_MEM;
	}
	else semaphore_acquire(&mbox_io_alloc_sem, NULL);
	
	int irq = irq_save();
	spinlock_acquire(&mbox->io_lock);
	spinlock_acquire(&mbox_io_alloc_lock);
	list_entry_t l = list_next(&mbox_io_free_list);
	list_del(l);
	spinlock_release(&mbox_io_alloc_lock);
	mbox_io_t io  = CONTAINER_OF(l, mbox_io_s, free_list);
	io->mbox      = mbox;
	io->ack_cb    = ack_cb;
	io->ack_data  = ack_data;

	if (list_empty(&mbox->io_recv_list))
	{
		io->send_cb   = send_cb;
		io->send_data = send_data;
		io->status    = MBOX_IO_STATUS_SEND_QUEUEING;

		list_add_before(&mbox->io_send_list, &io->io_list);

		spinlock_release(&mbox->io_lock);
		irq_restore(irq);
		/* Get for the io send link */
		mbox_get(mbox - mboxs);
	}
	else
	{
		list_entry_t rl = list_next(&mbox->io_recv_list);
		list_del(rl);
		spinlock_release(&mbox->io_lock);
		irq_restore(irq);

		io_ce_shadow_t shd = CONTAINER_OF(rl, io_ce_shadow_s, wait_list);
		
		io->iobuf     = shd->iobuf;
		io->iobuf_u   = shd->iobuf_u;
		io->status    = MBOX_IO_STATUS_PROCESSING;
		
		io_call_entry_t ce = &shd->proc->user_thread->ioce.head[shd->index];
		/* WRITE MESSAGE */
		ce->ce.data[0] = 0;
		ce->ce.data[1] = io->iobuf_u;
		ce->ce.data[2] = io - mbox_ios;
		send_cb(io, send_data, &ce->ce.data[3], &ce->ce.data[4]);
		user_thread_iocb_push(shd->proc, shd->index);
	}

	return 0;
}

int
mbox_io(mbox_t mbox, int ack_id, uintptr_t hint_a, uintptr_t hint_b, proc_t io_proc, iobuf_index_t io_index)
{
	cprintf("MBOX IO %d %d %016lx %016lx\n", mbox - mboxs, ack_id, hint_a, hint_b);
	int result;
	void *iobuf = NULL;
	uintptr_t iobuf_u;
	int irq;

	if (ack_id >= MBOX_IOS_MAX_COUNT)
	{
		result = -E_INVAL;
		goto nomore_io;
	}
	
	if (ack_id >= 0 && mbox_ios[ack_id].status == MBOX_IO_STATUS_PROCESSING)
	{
		mbox_io_t io = &mbox_ios[ack_id];
		io->status = MBOX_IO_STATUS_FINISHED;
		io->ack_cb(io, io->ack_data, hint_a, hint_b);
		
		iobuf   = io->iobuf;
		iobuf_u = io->iobuf_u;

		irq = irq_save();
		spinlock_acquire(&mbox_io_alloc_lock);
		list_add(&mbox_io_free_list, &mbox_ios[ack_id].free_list);
		spinlock_release(&mbox_io_alloc_lock);
		irq_restore(irq);
		/* Put for the ack */
		mbox_put(mbox);

		semaphore_release(&mbox_io_alloc_sem);
	}

	if (iobuf == NULL)
	{
		page_t p = page_alloc_atomic(MBOX_IO_IOBUF_PSIZE);
		if (p == NULL)
		{
			result = -E_INVAL;
			goto nomore_io;
		}
		
		iobuf = VADDR_DIRECT(PAGE_TO_PHYS(p));
		
		if (user_proc_arch_mmio_open(io_proc->user_proc, PADDR_DIRECT(iobuf), MBOX_IO_IOBUF_PSIZE << __PGSHIFT, &iobuf_u))
		{
			result = -E_INVAL;
			goto nomore_io;
		}
	}

	irq = irq_save();
	spinlock_acquire(&mbox->io_lock);
	
	if (list_empty(&mbox->io_send_list))
	{
		io_ce_shadow_t shd = &io_proc->user_thread->ioce.shadows[io_index];
		shd->proc     = io_proc;
		shd->index    = io_index;
		shd->mbox     = mbox;
		shd->iobuf    = iobuf;
		shd->iobuf_u  = iobuf_u;
		
		list_add_before(&mbox->io_recv_list, &shd->wait_list);
		spinlock_release(&mbox->io_lock);
		irq_restore(irq);
		/* Get for the shd link */
		mbox_get(mbox - mboxs);
	}
	else
	{
		list_entry_t l = list_next(&mbox->io_send_list);
		list_del(l);
		spinlock_release(&mbox->io_lock);
		irq_restore(irq);
		
		mbox_io_t io = CONTAINER_OF(l, mbox_io_s, io_list);
		io_call_entry_t ce = &io_proc->user_thread->ioce.head[io_index];
		mbox_send_callback_f send_cb = io->send_cb;
		void *send_data = io->send_data;
		io->status     = MBOX_IO_STATUS_PROCESSING;
		io->iobuf      = iobuf;
		io->iobuf_u    = iobuf_u;
		/* WRITE MESSAGE */
		ce->ce.data[0] = 0;
		ce->ce.data[1] = io->iobuf_u;
		ce->ce.data[2] = io - mbox_ios;
		
		send_cb(io, send_data, &ce->ce.data[3], &ce->ce.data[4]);
		user_thread_iocb_push(io_proc, io_index);
	}

	return 0;

  nomore_io:

	if (iobuf != NULL)
	{
		page_free_atomic(PHYS_TO_PAGE(PADDR_DIRECT(iobuf)));
	}

	if (iobuf_u != 0)
	{
		user_proc_arch_mmio_close(io_proc->user_proc, iobuf_u);
	}
	
	if (result) cprintf("MBOX IO FAILED\n");
	
	user_thread_iocb_push(io_proc, io_index);
	return result;
}


void
mbox_send_func(mbox_io_t io, void *__data, uintptr_t *hint_a, uintptr_t *hint_b)
{
	mbox_io_data_t data = __data;
	if (data->send_buf != NULL)
	{
		*hint_a = data->send_buf_size;
		
		if (*hint_a > (MBOX_IO_IOBUF_PSIZE << __PGSHIFT))
			*hint_a = (MBOX_IO_IOBUF_PSIZE << __PGSHIFT);

		memmove(io->iobuf, data->send_buf, *hint_a);
	}
	else *hint_a = 0;
	*hint_b = data->hint_send;
}

void
mbox_ack_func(mbox_io_t io, void *__data, uintptr_t hint_a, uintptr_t hint_b)
{
	mbox_io_data_t data = __data;
	if (data->recv_buf != NULL);
	{
		if (hint_a > data->recv_buf_size)
			hint_a = data->recv_buf_size;
		if (hint_a > (MBOX_IO_IOBUF_PSIZE << __PGSHIFT))
			hint_a = (MBOX_IO_IOBUF_PSIZE << __PGSHIFT);

		memmove(data->recv_buf, io->iobuf, hint_a);
	}
	data->hint_ack = hint_b;

	IPS_NODE_WAIT_CLEAR(data->ips);
	proc_notify(IPS_NODE_PTR(data->ips));
}
