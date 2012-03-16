#include "local.h"
#include "io.h"
#include "fiber.h"
#include "sync.h"

struct upriv_s init_upriv;

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
	f->io = 0;
	__current_fiber_set(f);

	io_init();	
}
