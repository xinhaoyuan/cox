#include <cpu.h>
#include <string.h>
#include <error.h>
#include <proc.h>
#include <irq.h>
#include <spinlock.h>
#include <malloc.h>

#include <lib/low_io.h>
#include <arch/context.h>

#include "sched/idle.h"
#include "sched/rr.h"

PLS_PTR_DEFINE(runqueue_s, __runqueue, NULL);
#define runqueue (PLS(__runqueue))

PLS_PTR_DEFINE(proc_s, __current, NULL);
#define current_set(proc) PLS_SET(__current, proc)

static inline void __schedule(int external);
static inline void __post_schedule(proc_t proc);

static int
sched_class_init(void)
{
	sched_class[SCHED_CLASS_IDLE] = &sched_class_idle;
	sched_class[SCHED_CLASS_RR]   = &sched_class_rr;
	return 0;
}

static int
sched_class_init_mp(void)
{
	int i;

	for (i = 0; i < SCHED_CLASS_COUNT; ++ i)
		sched_class[i]->init(runqueue);

	return 0;
}

static void
__proc_entry(void *arg)
{
	proc_t proc = current;
	proc->irq = 0;
	__post_schedule(proc);
	
	proc->entry(arg);

	cprintf("PROC[%016lx] %s END\n", proc, proc->name);
	while (1) __cpu_relax();
}

int
proc_init(proc_t proc, const char *name, int class, void (*entry)(void *arg), void *arg, uintptr_t stack_top)
{
	if (class == SCHED_CLASS_IDLE) return -E_INVAL;
	
	spinlock_init(&proc->lock);
	strncpy(proc->name, name, PROC_NAME_MAX);
	memset(&proc->sched_node, sizeof(sched_node_s), 0);
	proc->sched_node.class = sched_class[class];
	proc_timer_init(&proc->timer);
	proc->status = PROC_STATUS_WAITING;
	proc->entry  = entry;
	context_fill(&proc->ctx, __proc_entry, arg, stack_top);

	return 0;
}

void
proc_wait_pretend(void)
{
	current->status = PROC_STATUS_RUNNABLE_WEAK;
}

void
proc_wait_try(void)
{
	int irq = irq_save();
	proc_t proc = current;
	spinlock_acquire(&proc->lock);
	if (proc->status == PROC_STATUS_RUNNABLE_WEAK)
	{
		proc->status = PROC_STATUS_WAITING;
		/* lock would be released inside */
		__schedule(0);
	}
	else
	{
		proc->status = PROC_STATUS_RUNNABLE_WEAK;
		spinlock_release(&proc->lock);
	}
	irq_restore(irq);
}

void
proc_notify(proc_t proc)
{
	int irq = irq_save();
	spinlock_acquire(&proc->lock);
	if (proc->status == PROC_STATUS_RUNNABLE_WEAK)
	{
		proc->status = PROC_STATUS_RUNNABLE_STRONG;
	}
	else if (proc->status == PROC_STATUS_WAITING)
	{
		proc->status = PROC_STATUS_RUNNABLE_WEAK;
		/* Wake up to local */
		/* XXX: load balancing and local preference? */
		runqueue_t rq = runqueue;
		spinlock_acquire(&rq->lock);
		proc->sched_node.class->enqueue(rq, &proc->sched_node);
		spinlock_release(&rq->lock);
	}
	spinlock_release(&proc->lock);
	irq_restore(irq);
}

int
proc_delayed_self_notify_set(uintptr_t ticks)
{
	proc_t proc = current;	
	return proc_timer_set(&proc->timer, timer_tick + ticks);
}

int
proc_dsn_active(void)
{
	return current->timer.in;
}

static inline void
proc_switch(proc_t proc)
{
	proc_t prev = current;
	current_set(proc);
	proc->sched_prev = prev;

	if (prev != proc)
		context_switch(&prev->ctx, &proc->ctx);
}

/* Initialize context support and sched classes */
int
sched_init(void)
{
	int ret;
	if ((ret = context_init())) return ret;
	if ((ret = sched_class_init())) return ret;

	return 0;
}

/* Initialize local data for scheduler and idle proc */
int
sched_init_mp(void)
{
	int ret;
	
	proc_t idle = kmalloc(sizeof(proc_s));
	if (idle == NULL) return -E_NO_MEM;
	
	runqueue_t rq = kmalloc(sizeof(runqueue_s));
	if (rq == NULL) return -E_NO_MEM;
	
	PLS_SET(__runqueue, rq);
	
	if ((ret = sched_class_init_mp())) return ret;

	spinlock_init(&idle->lock);
   	idle->status = PROC_STATUS_RUNNABLE_WEAK;
	idle->sched_node.class = &sched_class_idle;
	
	rq->idle.node = &idle->sched_node;
	current_set(idle);
	
	return 0;
}

/* Define the strategy for pick proc from each sched class */
static inline sched_node_t
rq_pick(runqueue_t rq)
{
	sched_node_t node;
	if ((node = sched_class_rr.pick(rq))) return node;
	return sched_class_idle.pick(rq);
}

static inline void
__schedule(int external)
{
	int irq = irq_save();
	proc_t self = current;
	runqueue_t rq = runqueue;
	if (external) spinlock_acquire(&self->lock);
	spinlock_acquire(&rq->lock);

	self->irq = irq;

	if (self->status == PROC_STATUS_RUNNABLE_WEAK ||
		self->status == PROC_STATUS_RUNNABLE_STRONG)
		self->sched_node.class->enqueue(rq, &self->sched_node);
	
	sched_node_t next_sched_node = rq_pick(rq);
	proc_t next;

	next_sched_node->class->dequeue(rq, next_sched_node);
	next = CONTAINER_OF(next_sched_node, proc_s, sched_node);

	proc_switch(next);
	/* Maybe on different CPU */
	rq = runqueue;

	__post_schedule(current);
}

static inline void
__post_schedule(proc_t proc)
{
	proc->switched = 1;
	spinlock_release(&proc->sched_prev->lock);
	spinlock_release(&runqueue->lock);
	irq_restore(proc->irq);
}

void
schedule(void) { __schedule(1); }
