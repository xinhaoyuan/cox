#include <mbox.h>
#include <irq.h>
#include <spinlock.h>
#include <error.h>
#include <string.h>
#include <page.h>
#include <user.h>

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
mbox_io_send(mbox_io_t io, ips_node_t ips, void *buf, size_t buf_size, uintptr_t hint)
{
	ips_node_s __ips;
	if (ips == NULL)
	{
		ips = &__ips;
		IPS_NODE_PTR_SET(ips, current);
		IPS_NODE_WAIT_SET(ips);
	}
	
	io->ips = ips;
	io->status = MBOX_IO_STATUS_PROCESSING;
	io_call_entry_t ce = &io->io_proc->user_thread->ioce.head[io->io_index];
	/* WRITE MESSAGE */
	if (buf_size > (MBOX_IO_UBUF_PSIZE << __PGSHIFT))
		buf_size = (MBOX_IO_UBUF_PSIZE << __PGSHIFT);
	ce->ce.data[0] = 0;
	ce->ce.data[1] = io->ubuf_u;
	ce->ce.data[2] = io - mbox_ios;
	ce->ce.data[3] = buf_size;
	ce->ce.data[4] = hint;
	io->buf = buf;
	io->buf_size = buf_size;
	user_thread_iocb_push(io->io_proc, io->io_index);

	if (ips == &__ips)
	{
		while (IPS_NODE_WAIT(ips))
			proc_wait_try();
	}

	return 0;
}

int
mbox_io(int mbox_id, int ack, size_t ack_size, proc_t io_proc, iobuf_index_t io_index)
{
	cprintf("MBOX IO\n");
	int result;
	void *ubuf = NULL;
	uintptr_t ubuf_u;
	
	if (ack != -1 && mbox_ios[ack].status == MBOX_IO_STATUS_PROCESSING)
	{
		if (mbox_ios[ack].buf_size > 0)
		{
			/* if the ack io has a read buf, copy read data into up level buffer */
			if (ack_size > mbox_ios[ack].buf_size) ack_size = mbox_ios[ack].buf_size;
			memmove(mbox_ios[ack].buf, mbox_ios[ack].ubuf, ack_size);
		}
		
		mbox_ios[ack].status = MBOX_IO_STATUS_FINISHED;
		IPS_NODE_WAIT_CLEAR(mbox_ios[ack].ips);
		proc_notify(IPS_NODE_PTR(mbox_ios[ack].ips));
		
		ubuf = mbox_ios[ack].ubuf;
		ubuf_u = mbox_ios[ack].ubuf_u;
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
	if (ubuf != NULL)
	{
		list_add(&mbox_io_free_list, &mbox_ios[ack].free_list);
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

	if (ubuf == NULL)
	{
		page_t p = page_alloc_atomic(MBOX_IO_UBUF_PSIZE);
		if (p == NULL)
		{
			result = -E_INVAL;
			goto nomore_io;
		}
		
		ubuf = VADDR_DIRECT(PAGE_TO_PHYS(p));
		
		if (user_proc_arch_mmio_open(io_proc->user_proc, PADDR_DIRECT(ubuf), MBOX_IO_UBUF_PSIZE << __PGSHIFT, &ubuf_u))
		{
			result = -E_INVAL;
			goto nomore_io;
		}
	}
	else mbox_put(mbox_id);

	mbox_io_t io = CONTAINER_OF(l, mbox_io_s, free_list);
	io->status   = MBOX_IO_STATUS_QUEUEING;
	io->ips      = NULL;
	io->io_proc  = io_proc;
	io->io_index = io_index;
	io->mbox     = mbox;
	io->ubuf     = ubuf;
	io->ubuf_u   = ubuf_u;

	spinlock_acquire(&mbox->io_lock);
	list_add(&mbox->io_list, &io->io_list);
	spinlock_release(&mbox->io_lock);
	semaphore_release(&mbox->io_sem);
	
	return 0;

  nomore_io:
	if (ubuf != NULL)
	{
		page_free_atomic(PHYS_TO_PAGE(PADDR_DIRECT(ubuf)));
		if (mbox) mbox_put(mbox_id);
	}
	if (result) cprintf("MBOX IO FAILED\n");
	return result;
}
