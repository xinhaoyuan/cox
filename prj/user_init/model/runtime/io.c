#include <string.h>
#include <user/arch/iocb.h>
#include "io.h"
#include "fiber.h"

inline static iobuf_index_t ioce_alloc_unsafe(void);
inline static void ioce_advance_unsafe(void);
inline static void ioce_free_unsafe(iobuf_index_t idx);

void
__iocb(void *ret)
{
	if (!__io_ubusy)
	{
		__io_ubusy_set(1);
		ips_node_t ips;
		fiber_t fiber;
		io_call_entry_t entry = __ioce_head;
		iobuf_index_t i = __iocb_head, tail = __iocb_tail;
		iobuf_index_t ce;
		upriv_t p = __upriv;
		while (i != tail)
		{
			ce    = __iocb_entry[i];
			ips   = (ips_node_t)entry[ce].ce.data[IO_ARGS_COUNT - 1];
			
			if (ips)
			{
				fiber = IPS_NODE_PTR(ips);
				IPS_NODE_WAIT_CLEAR(ips);
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

	io_b(NULL, 2, IO(IO_SET_CALLBACK, (uintptr_t)__iocb));
}

int
io_b(ips_node_t node, int argc, uintptr_t args[])
{
	if (argc >= IO_ARGS_COUNT - 1)
	{
		/* too many args */
		return -1;
	}

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
	
	memmove(ce->ce.data, args, sizeof(uintptr_t) * argc);
	ce->ce.data[IO_ARGS_COUNT - 1] = (uintptr_t)node;
	ce->ce.status = IO_CALL_STATUS_WAIT;

	if (node)
	{
		IPS_NODE_WAIT_SET(node);
		IPS_NODE_AC_WAIT_SET(node);
		IPS_NODE_PTR_SET(node, __current_fiber);
	}

	ioce_advance_unsafe();

	__io_restore(io);

	return 0;
}

void
debug_putchar(int ch)
{
	io_b(NULL, 2, IO(IO_DEBUG_PUTCHAR, ch));
}
