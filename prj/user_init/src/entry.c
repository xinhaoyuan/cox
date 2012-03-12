#include <string.h>
#include <lib/low_io.h>

#include <user/io.h>
#include <user/tls.h>
#include <user/local.h>
#include <user/syscall.h>
#include <user/iocb.h>

#define TLS_IOCE_HEAD  (TLS_USTART)
#define TLS_IOCE_CAP   (TLS_USTART + 1)
#define TLS_IOCB_ENTRY (TLS_USTART + 2)
#define TLS_IOCB_CAP   (TLS_USTART + 3)

void
__iocb(void *ret)
{
	TLS_SET(TLS_IOCB_HEAD, TLS(TLS_IOCB_TAIL));
	if (ret != NULL)
	{
		TLS_SET(TLS_IOCB_BUSY, 2);
		iocb_return(ret);
	}
	else TLS_SET(TLS_IOCB_BUSY, 0);
}

void
io_init(void)
{
	io_call_entry_t entry = (io_call_entry_t)TLS(TLS_IOCE_HEAD);
	int cap = TLS(TLS_IOCE_CAP);
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
}

iobuf_index_t
ioce_alloc(void)
{
	io_call_entry_t entry = (io_call_entry_t)TLS(TLS_IOCE_HEAD);
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

void
ioce_free(iobuf_index_t idx)
{
	io_call_entry_t entry = (io_call_entry_t)TLS(TLS_IOCE_HEAD);

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
	
int
io_a1(uintptr_t arg0)
{
	io_call_entry_t entry = (io_call_entry_t)TLS(TLS_IOCE_HEAD);
	io_call_entry_t ce = entry + ioce_alloc();

	if (ce == entry) return -1;

	ce->ce.data[0] = arg0;
	ce->ce.status = IO_CALL_STATUS_WAIT;

	entry->head.tail = ce->ce.next;

	return 0;
}

int
io_a2(uintptr_t arg0, uintptr_t arg1)
{
	io_call_entry_t entry = (io_call_entry_t)TLS(TLS_IOCE_HEAD);
	io_call_entry_t ce = entry + ioce_alloc();

	if (ce == entry) return -1;

	ce->ce.data[0] = arg0;
	ce->ce.data[1] = arg1;
	ce->ce.status = IO_CALL_STATUS_WAIT;

	entry->head.tail = ce->ce.next;

	return 0;
}

void
__cputchar(int c)
{
	io_a2(IO_DEBUG_PUTCHAR, c);
}


void
entry(tls_t __tls)
{
	tls_s tls = *__tls;
	TLS_SET(TLS_IOCE_HEAD, (uintptr_t)tls.info.ioce.head);
	TLS_SET(TLS_IOCE_CAP, tls.info.ioce.cap);
	TLS_SET(TLS_IOCB_ENTRY, (uintptr_t)tls.info.iocb.entry);
	TLS_SET(TLS_IOCB_CAP, tls.info.iocb.cap);

	io_init();
	io_a2(IO_SET_CALLBACK, (uintptr_t)__iocb);

	low_io_putc = __cputchar;
	cprintf("Hello world\n");
	
	while (1) ;
}
