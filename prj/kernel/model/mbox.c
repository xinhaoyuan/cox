#include <mbox.h>
#include <irq.h>
#include <spinlock.h>
#include <error.h>
#include <string.h>
#include <page.h>
#include <user.h>
#include <proc.h>

static list_entry_s mbox_free_list;
static spinlock_s   mbox_alloc_lock;

static list_entry_s mbox_io_free_list;
static spinlock_s   mbox_io_alloc_lock;

mbox_s mboxs[MBOXS_MAX_COUNT];
mbox_io_s mbox_ios[MBOX_IOS_MAX_COUNT];

static inline mbox_t mbox_get(int mbox_id) __attribute__((always_inline));
static inline mbox_t mbox_get(int mbox_id) { return mboxs + mbox_id; }

static inline void mbox_put(int mbox_id) __attribute__((always_inline));
static inline void mbox_put(int mbox_id) { }

int
mbox_init(void)
{
	list_init(&mbox_free_list);
	list_init(&mbox_io_free_list);

	spinlock_init(&mbox_alloc_lock);
	spinlock_init(&mbox_io_alloc_lock);

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

		mbox->status = MBOX_STATUS_CLOSED;
		mbox->proc   = proc;

		list_init(&mbox->io_list);
		spinlock_init(&mbox->io_lock);
		semaphore_init(&mbox->io_sem, 0);

		return mbox - mboxs;
	}
	else return -1;
}

void
mbox_free(int mbox_id)
{
	/* XXX */
}

mbox_io_t
mbox_io_acquire(int mbox_id)
{
	mbox_t mbox = mbox_get(mbox_id);
	if (mbox == NULL) return NULL;

	/* XXX timeout func */
	semaphore_acquire(&mbox->io_sem, NULL);
	spinlock_acquire(&mbox->io_lock);
	
	list_entry_t l = NULL;
	if (!list_empty(&mbox->io_list))
	{
		l = list_next(&mbox->io_list);
		list_del(l);
	}

	spinlock_release(&mbox->io_lock);

	if (l == NULL)
	{
		/* Should not happen */
		mbox_put(mbox_id);
		return NULL;
	}

	mbox_io_t io = CONTAINER_OF(l, mbox_io_s, io_list);
	mbox_put(mbox_id);
	
	return io;
}

int
mbox_io_send(mbox_io_t io, ack_callback_f ack_cb, void *ack_data, uintptr_t hint_a, uintptr_t hint_b)
{
	
	io->ack_cb = ack_cb;
	io->ack_data = ack_data;
	
	io->status = MBOX_IO_STATUS_PROCESSING;
	io_call_entry_t ce = &io->io_proc->user_thread->ioce.head[io->io_index];
	/* WRITE MESSAGE */
	ce->ce.data[0] = 0;
	ce->ce.data[1] = io->iobuf_u;
	ce->ce.data[2] = io - mbox_ios;
	ce->ce.data[3] = hint_a;
	ce->ce.data[4] = hint_b;
	user_thread_iocb_push(io->io_proc, io->io_index);

	return 0;
}

int
mbox_io(int mbox_id, int ack_id, uintptr_t hint_a, uintptr_t hint_b, proc_t io_proc, iobuf_index_t io_index)
{
	cprintf("MBOX IO %d %d %016lx %016lx\n", mbox_id, ack_id, hint_a, hint_b);
	int result;
	void *iobuf = NULL;
	uintptr_t iobuf_u;
	
	if (ack_id != -1 && mbox_ios[ack_id].status == MBOX_IO_STATUS_PROCESSING)
	{
		mbox_io_t io = &mbox_ios[ack_id];
		io->status = MBOX_IO_STATUS_FINISHED;
		io->ack_cb(io, io->ack_data, hint_a, hint_b);
		
		iobuf   = io->iobuf;
		iobuf_u = io->iobuf_u;
	}

	list_entry_t l = NULL;
	mbox_t mbox = mbox_get(mbox_id);

	if (mbox == NULL)
	{
		result = -E_INVAL;
		goto nomore_io;
	}
	
	int irq = irq_save();
	spinlock_acquire(&mbox_io_alloc_lock);

	/* free the ack io */
	if (iobuf != NULL)
	{
		list_add(&mbox_io_free_list, &mbox_ios[ack_id].free_list);
	}

	if (!list_empty(&mbox_io_free_list))
	{
		l = list_next(&mbox_io_free_list);
		list_del(l);
	}

	spinlock_release(&mbox_io_alloc_lock);
	irq_restore(irq);

	if (l == NULL)
	{
		result = -E_INVAL;
		goto nomore_io;
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
	else mbox_put(mbox_id);

	mbox_io_t io = CONTAINER_OF(l, mbox_io_s, free_list);
	io->status   = MBOX_IO_STATUS_QUEUEING;
	io->io_proc  = io_proc;
	io->io_index = io_index;
	io->mbox     = mbox;
	io->iobuf    = iobuf;
	io->iobuf_u  = iobuf_u;

	spinlock_acquire(&mbox->io_lock);
	list_add(&mbox->io_list, &io->io_list);
	spinlock_release(&mbox->io_lock);
	semaphore_release(&mbox->io_sem);
	
	return 0;

  nomore_io:
	if (iobuf != NULL)
	{
		page_free_atomic(PHYS_TO_PAGE(PADDR_DIRECT(iobuf)));
		if (mbox) mbox_put(mbox_id);
	}
	if (result) cprintf("MBOX IO FAILED\n");
	return result;
}

void
mbox_ack_func(mbox_io_t io, void *__data, uintptr_t hint_a, uintptr_t hint_b)
{
	mbox_ack_data_t data = __data;
	if (data->buf != NULL);
	{
		if (hint_a > data->buf_size)
			hint_a = data->buf_size;
		if (hint_a > (MBOX_IO_IOBUF_PSIZE << __PGSHIFT))
			hint_a = (MBOX_IO_IOBUF_PSIZE << __PGSHIFT);

		memmove(data->buf, io->iobuf, hint_a);
	}

	IPS_NODE_WAIT_CLEAR(data->ips);
	proc_notify(IPS_NODE_PTR(data->ips));
}
