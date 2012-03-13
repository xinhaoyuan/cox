#include <string.h>
#include <user/memlayout.h>
#include <user/io.h>
#include <user/tls.h>
#include <user/local.h>
#include <user/syscall.h>
#include <user/iocb.h>

#include "runtime.h"

struct upriv_s init_upriv;

#define __iocb_busy_set(v)          TLS_SET(TLS_IOCB_BUSY,(uintptr_t)(v))
#define __iocb_head_set(v)          TLS_SET(TLS_IOCB_HEAD,(uintptr_t)(v))
#define __iocb_tail_set(v)          TLS_SET(TLS_IOCB_TAIL,(uintptr_t)(v))
#define __ioce_head_set(v)          TLS_SET(TLS_IOCE_HEAD,(uintptr_t)(v))
#define __ioce_cap_set(v)           TLS_SET(TLS_IOCE_CAP,(uintptr_t)(v))
#define __iocb_entry_set(v)         TLS_SET(TLS_IOCB_ENTRY,(uintptr_t)(v))
#define __iocb_cap_set(v)           TLS_SET(TLS_IOCB_CAP,(uintptr_t)(v))
#define __io_ubusy_set(v)           TLS_SET(TLS_IO_UBUSY,(uintptr_t)(v))
#define __current_fiber_set(v)      TLS_SET(TLS_CURRENT_FIBER,(uintptr_t)(v))
#define __upriv_set(v)              TLS_SET(TLS_UPRIV,(uintptr_t)(v))

/* IO PROCESS CODE ============================================ */

static void
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

inline static int
__io_save(void)
{
	int r = __io_ubusy;
	__io_ubusy_set(1);
	return r;
}

inline static void
__io_restore(int status)
{
	if (status == 0 && __iocb_busy)
	{
		__io_ubusy_set(status);
		__iocb(NULL);
	}
}

int io_save(void) { return __io_save(); }
void io_restore(int status) { return __io_restore(status); }

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


static void
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
}

void
fiber_schedule(void)
{
	int io = __io_save();

	upriv_t p = __upriv;
	fiber_t f = __current_fiber;

	if (f->status == FIBER_STATUS_RUNNABLE_WEAK ||
		f->status == FIBER_STATUS_RUNNABLE_STRONG)
	{
		f->sched_node.next = &p->sched_node;
		f->sched_node.prev = p->sched_node.prev;

		f->sched_node.next->prev = &f->sched_node;
		f->sched_node.prev->next = &f->sched_node;
	}

	sched_node_t next;
	if ((next = p->sched_node.next) == &p->sched_node)
		next = &p->idle_fiber.sched_node;
	else
	{
		next->next->prev = next->prev;
		next->prev->next = next->next;
	}

	fiber_t n = CONTAINER_OF(next, fiber_s, sched_node);

	__current_fiber_set(n);
	context_switch(&f->ctx, &n->ctx);

	/* XXX migrate? */
	
	__io_restore(io);
}

void
thread_init(tls_t tls)
{
	tls_s __tls = *tls;
	__ioce_head_set(__tls.info.ioce.head);
	__ioce_cap_set(__tls.info.ioce.cap);
	__iocb_entry_set(__tls.info.iocb.entry);
	__iocb_cap_set(__tls.info.iocb.cap);

	if (__thread_arg == 0)
	{
		__upriv_set(&init_upriv);
	}

	upriv_t p = __upriv;
	fiber_t f = &p->idle_fiber;
	p->sched_node.next = p->sched_node.prev = &p->sched_node;
	f->status = FIBER_STATUS_RUNNABLE_IDLE;
	__current_fiber_set(f);

	io_init();
	io_a2(IO_SET_CALLBACK, (uintptr_t)__iocb);
}

static void
__fiber_entry(void *arg)
{
	fiber_t f = __current_fiber;
	f->entry(arg);

	__io_save();
	f->status = FIBER_STATUS_ZOMBIE;
	while (1)
		fiber_schedule();
}

void
fiber_init(fiber_t fiber, fiber_entry_f entry, void *arg, void *stack, size_t stack_size)
{
	fiber->status = FIBER_STATUS_RUNNABLE_WEAK;
	fiber->entry = entry;
	context_fill(&fiber->ctx, __fiber_entry, arg, (uintptr_t)ARCH_STACKTOP(stack, stack_size));

	int io = __io_save();
	upriv_t p = __upriv;
	
	fiber->sched_node.next = &p->sched_node;
	fiber->sched_node.prev = p->sched_node.prev;
	fiber->sched_node.next->prev = &fiber->sched_node;
	fiber->sched_node.prev->next = &fiber->sched_node;

	__io_restore(io);
}

void
debug_putchar(int ch)
{
	io_a2(IO_DEBUG_PUTCHAR, ch);
}
