#include <string.h>
#include <user/iocb.h>
#include "io.h"
#include "fiber.h"

void
__iocb(void *ret)
{
	if (!__io_ubusy)
	{
		__io_ubusy_set(1);
		__iocb_head_set(__iocb_tail);
		__io_ubusy_set(0);
	}
	
	if (ret != NULL)
	{
		TLS_SET(TLS_IOCB_BUSY, 2);
		iocb_return(ret);
	}
	else TLS_SET(TLS_IOCB_BUSY, 0);
}

inline static iobuf_index_t
ioce_alloc_unsafe(void)
{
	int io = __io_save();
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
	__io_restore(io);
	
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

inline static int
io_a1(uintptr_t arg0)
{
	int io = __io_save();
	io_call_entry_t entry = __ioce_head;
	io_call_entry_t ce = entry + ioce_alloc_unsafe();

	if (ce == entry) {
		__io_restore(io);
		return -1;
	}

	ce->ce.data[0] = arg0;
	ce->ce.status = IO_CALL_STATUS_WAIT;

	ioce_advance_unsafe();

	__io_restore(io);
	return 0;
}

inline static int
io_a2(uintptr_t arg0, uintptr_t arg1)
{
	int io = __io_save();
	io_call_entry_t entry = __ioce_head;
	io_call_entry_t ce = entry + ioce_alloc_unsafe();

	if (ce == entry) {
		__io_restore(io);
		return -1;
	}

	ce->ce.data[0] = arg0;
	ce->ce.data[1] = arg1;
	ce->ce.status = IO_CALL_STATUS_WAIT;

	ioce_advance_unsafe();

	__io_restore(io);
	return 0;
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

	if (node == NULL)
	{
		ips_node_s __node;
		io_b(&__node, argc, args);
		ips_wait(&__node);
		return 0;
	}

	int io = __io_save();

	upriv_t p = __upriv;
	if (semaphore_acquire(&p->io_sem, node))
		ips_wait(node);
	
	io_call_entry_t entry = __ioce_head;
	io_call_entry_t ce = entry + ioce_alloc_unsafe();
	
	memmove(ce->ce.data, args, sizeof(uintptr_t) * argc);
	ce->ce.data[IO_ARGS_COUNT - 1] = (uintptr_t)node;
	ce->ce.status = IO_CALL_STATUS_WAIT;

	ioce_advance_unsafe();

	__io_restore(io);

	return 0;
}

void
debug_putchar(int ch)
{
	io_b(NULL, 2, IO(IO_DEBUG_PUTCHAR, ch));
}
