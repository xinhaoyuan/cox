#include <string.h>
#include <user/arch/iocb.h>
#include <runtime/io.h>
#include <runtime/fiber.h>

inline static iobuf_index_t ioce_alloc_unsafe(void);
inline static void ioce_advance_unsafe(void);
inline static void ioce_free_unsafe(iobuf_index_t idx);

void
__iocb(void *ret)
{
	if (!__io_ubusy)
	{
		__io_ubusy_set(1);
		io_data_t iod;
		fiber_t fiber;
		io_call_entry_t entry = __ioce_head;
		iobuf_index_t i = __iocb_head, tail = __iocb_tail;
		iobuf_index_t ce;
		upriv_t p = __upriv;
		while (i != tail)
		{
			ce    = __iocb_entry[i];
			iod   = (io_data_t)entry[ce].ce.data[IO_ARGS_COUNT - 1];
			
			if (iod)
			{
				fiber = (fiber_t)IO_DATA_PTR(iod);
				memmove(iod->io, entry[ce].ce.data, sizeof(uintptr_t) * iod->retc);
				
				IO_DATA_WAIT_CLEAR(iod);
				if (fiber != NULL) fiber_notify(fiber);
			}
			
			ioce_free_unsafe(ce);
			semaphore_release(&p->io_sem);
			
			i = (i + 1) % __iocb_cap;
		}

		__iocb_head_set(tail);
		
		__io_ubusy_set(0);

		if (ret != NULL) __iocb_busy_set(2);
		else __iocb_busy_set(0);
	}
	
	if (ret != NULL)
		iocb_return(ret);
}

inline static iobuf_index_t
ioce_alloc_unsafe(void)
{
	io_call_entry_t entry = __ioce_head;
	iobuf_index_t idx = entry->head.tail;

	if (idx != 0)
	{			
		/* cycle dlist remove */
		entry[entry[idx].ce.next].ce.prev = entry[idx].ce.prev;
		entry[entry[idx].ce.prev].ce.next = entry[idx].ce.next;

		if (entry[idx].ce.next == idx)
			entry[idx].ce.next = 0;
	}
	
	return idx;
}

inline static void
ioce_advance_unsafe(void)
{
	io_call_entry_t entry = __ioce_head;
	entry->head.tail = entry[entry->head.tail].ce.next;
}

inline static void
ioce_free_unsafe(iobuf_index_t idx)
{
	io_call_entry_t entry = __ioce_head;

	if (entry->head.tail == 0)
	{
		entry[idx].ce.next = idx;
		entry[idx].ce.prev = idx;
		entry->head.head = idx;
		entry->head.tail = idx;
	}
	else
	{
		/* cycle dlist insert */
		entry[idx].ce.next = entry->head.tail;
		entry[idx].ce.prev = entry[entry->head.tail].ce.prev;
		entry[entry[idx].ce.next].ce.prev = idx;
		entry[entry[idx].ce.prev].ce.next = idx;
	}
}

void
io_init(void)
{
	io_call_entry_t entry = __ioce_head;
	int cap = __ioce_cap;
	iobuf_index_t i;
	for (i = 1; i < cap; ++ i)
	{
		entry[i].ce.next = i + 1;
		entry[i].ce.prev = i - 1;
	}

	entry[cap - 1].ce.next = 1;
	entry[1].ce.prev = cap - 1;

	entry->head.tail = 1;
	entry->head.head = 1;

	semaphore_init(&__upriv->io_sem, __ioce_cap - 1);
	__io_ubusy_set(0);

	io_data_s iocb_set = IO_DATA_INITIALIZER(IO_SET_CALLBACK, (uintptr_t)__iocb);
	io(&iocb_set, IO_MODE_SYNC);
}

int
io(io_data_t iod, int mode)
{
	int io = __io_save();
	ips_node_s __node;
	upriv_t p = __upriv;
	if (semaphore_acquire(&p->io_sem, &__node))
	{
		__io_ubusy_set(0);
		ips_wait(&__node);
		__io_ubusy_set(1);
	}

	
	io_call_entry_t entry = __ioce_head;
	io_call_entry_t ce = entry + ioce_alloc_unsafe();

	memmove(ce->ce.data, iod->io, sizeof(uintptr_t) * iod->argc);
	ce->ce.status = IO_CALL_STATUS_WAIT;
	if (mode != IO_MODE_NO_RET)
	{
		ce->ce.data[IO_ARG_UD] = (uintptr_t)iod;
		IO_DATA_WAIT_SET(iod);
		IO_DATA_PTR_SET(iod, __current_fiber);
	}
	else ce->ce.data[IO_ARG_UD] = 0;
	ioce_advance_unsafe();

	__io_restore(io);

	if (mode == IO_MODE_SYNC)
	{
		while (IO_DATA_WAIT(iod))
			fiber_wait_try();
	}
	
	return 0;
}
