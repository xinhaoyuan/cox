#define DEBUG_COMPONENT DBG_SCHED

#include <asm/cpu.h>
#include <string.h>
#include <error.h>
#include <frame.h>
#include <irq.h>
#include <spinlock.h>
#include <arch/memlayout.h>
#include <arch/context.h>
#include <arch/irq.h>
#include <user.h>

#include "sched/idle.h"
#include "sched/rr.h"

PLS_PTR_DEFINE(runqueue_s, __runqueue, NULL);
#define runqueue (PLS(__runqueue))

proc_s __current_init =
{
    .name = "__CURRENT_INIT",
};

PLS_PTR_DEFINE(proc_s, __current, &__current_init);
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

  die:
    spinlock_acquire(&proc->lock);
    proc->status = PROC_STATUS_ZOMBIE;
    __schedule(0);

    DEBUG("PROC %p is dead. Should not be here.\n", proc);
    goto die;
}

int
proc_init(proc_t proc, const char *name, int class, void (*entry)(void *arg), void *arg, void *stack_base, size_t stack_size)
{
    if (class == SCHED_CLASS_IDLE) return -E_INVAL;
    
    spinlock_init(&proc->lock);
    strncpy(proc->name, name, PROC_NAME_MAX);
    memset(&proc->sched_node, sizeof(sched_node_s), 0);
    proc->sched_node.class = sched_class[class];
    proc->status = PROC_STATUS_WAITING;
    proc->entry  = entry;
    proc->type   = PROC_TYPE_KERN;
    context_fill(&proc->ctx, __proc_entry, arg,
                 (uintptr_t)ARCH_STACKTOP(stack_base, stack_size));
    
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

static inline void
proc_switch(proc_t proc)
{
    proc_t prev = current;  
    proc->sched_prev = prev;
    if (prev->type != PROC_TYPE_USER)
        proc->sched_prev_usr = prev->sched_prev_usr;
    else if (prev->status != PROC_STATUS_RUNNABLE_WEAK ||
             prev->status != PROC_STATUS_RUNNABLE_STRONG)
    {
        /* prev no longer in current rq, save the user context */
        user_thread_save_context(prev, USER_THREAD_CONTEXT_HINT_LAZY);
        proc->sched_prev_usr = NULL;
    }
    else proc->sched_prev_usr = prev;

    current_set(proc);
    if (prev != proc)
    {
        context_switch(&prev->ctx, &proc->ctx);
    }
}

/* Initialize context support and sched classes */
int
sched_sys_init(void)
{
    int ret;
    if ((ret = sched_class_init())) return ret;

    return 0;
}

PLS_PREALLOC(proc_s, __idle);
PLS_PREALLOC(runqueue_s, __rq);

/* Initialize local data for scheduler and idle proc */
int
sched_sys_init_mp(void)
{
    int ret;
    
    proc_t idle   = PLS_PREALLOC_PTR(__idle);
    runqueue_t rq = PLS_PREALLOC_PTR(__rq);
    
    PLS_SET(__runqueue, rq);
    
    if ((ret = sched_class_init_mp())) return ret;

    spinlock_init(&idle->lock);
    idle->status = PROC_STATUS_RUNNABLE_WEAK;
    idle->sched_node.class = &sched_class_idle;
    idle->sched_prev_usr = NULL;
    idle->type = PROC_TYPE_KERN;
    
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
    
    /* May be on different CPU now */
    /* Should retrive the PLS variable again to update */
    __post_schedule(self);
}

static inline void
__post_schedule(proc_t proc)
{
    spinlock_release(&proc->sched_prev->lock);
    spinlock_release(&runqueue->lock);
    irq_restore(proc->irq);
}

void
schedule(void) { __schedule(1); }

void
yield(void) { schedule(); }
