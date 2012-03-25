#include <runtime/local.h>
#include <runtime/io.h>
#include <runtime/fiber.h>
#include <runtime/sync.h>
#include <runtime/page.h>
#include <runtime/malloc.h>
#include <lib/low_io.h>
#include <user/arch/syscall.h>
#include <user/init.h>
#include <mach.h>

struct upriv_s init_upriv;

void entry(void);

void
__init(tls_t tls, uintptr_t start, uintptr_t end)
{
	low_io_putc = __debug_putc;
	
	tls_s __tls = *tls;
	__ioce_head_set(__tls.info.ioce.head);
	__ioce_cap_set(__tls.info.ioce.cap);
	__iocb_entry_set(__tls.info.iocb.entry);
	__iocb_cap_set(__tls.info.iocb.cap);

	if (__thread_arg == 0)
	{
		/* Init thread of a process */
		page_init((void *)(start + (__bin_end - __bin_start)), (void *)end);
		size_t tls_pages = (end - (start + (__bin_end - __bin_start))) >> __PGSHIFT;
		/* XXX: use a dummy palloc to occupy the tls space in heap */
		(void)palloc(tls_pages);
		(void)malloc_init();
		__upriv_set(&init_upriv);
	}

	upriv_t p = __upriv;
	fiber_t f = &p->idle_fiber;
	p->sched_node.next = p->sched_node.prev = &p->sched_node;
	f->status = FIBER_STATUS_RUNNABLE_IDLE;
	f->io = 0;
	__current_fiber_set(f);

	io_init();
	entry();
}
