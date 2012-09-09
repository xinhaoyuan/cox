#include <string.h>
#include <user/io.h>
#include <user/arch/syscall.h>
#include <user/arch/memlayout.h>

#include <runtime/fiber.h>
#include <runtime/io.h>

void
fiber_sys_init(void)
{
    upriv_t p = __upriv;
    fiber_t f = &p->idle_fiber;
    p->sched_node.next = p->sched_node.prev = &p->sched_node;
    f->status = FIBER_STATUS_RUNNABLE_IDLE;
    __current_fiber_set(f);
}

static inline void
__post_schedule(fiber_t f)
{ }

void
fiber_schedule(void)
{
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

    __post_schedule(n);

    if (n != f)
    {
        __current_fiber_set(n);
        context_switch(&f->ctx, &n->ctx);
    }
}

static void
__fiber_entry(void *arg)
{
    fiber_t f = __current_fiber;
    __post_schedule(f);
    f->entry(arg);

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

    upriv_t p = __upriv;
    
    fiber->sched_node.next = &p->sched_node;
    fiber->sched_node.prev = p->sched_node.prev;
    fiber->sched_node.next->prev = &fiber->sched_node;
    fiber->sched_node.prev->next = &fiber->sched_node;
}

void
fiber_wait_try(void)
{
    fiber_t f = __current_fiber;
    switch (f->status)
    {
    case FIBER_STATUS_RUNNABLE_WEAK:
        f->status = FIBER_STATUS_WAIT;
        fiber_schedule();
        break;

    case FIBER_STATUS_RUNNABLE_STRONG:
        f->status = FIBER_STATUS_RUNNABLE_WEAK;
        break;
    }
}

void
fiber_notify(fiber_t f)
{
    /* XXX: remote notify */
    if (f->status == FIBER_STATUS_RUNNABLE_WEAK)
    {
        f->status = FIBER_STATUS_RUNNABLE_STRONG;
    }
    else if (f->status == FIBER_STATUS_WAIT)
    {
        f->status = FIBER_STATUS_RUNNABLE_WEAK;
        upriv_t p = __upriv;
        f->sched_node.next = &p->sched_node;
        f->sched_node.prev = p->sched_node.prev;

        f->sched_node.next->prev = &f->sched_node;
        f->sched_node.prev->next = &f->sched_node;
    }
}