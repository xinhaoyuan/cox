#include <timer.h>
#include <irq.h>
#include <proc.h>
#include <arch/local.h>
#include <arch/irq.h>

static int __tick_handler(int irq_no, uint64_t acc);
tick_t timer_tick;
static spinlock_s timer_lock;
static crh_s crh;

PLS_ATOM_DEFINE(int, __timer_master_cpu, 0);

void
timer_master_cpu_set(void)
{
    PLS_SET(__timer_master_cpu, 1);
}


int
timer_sys_init(void)
{
    spinlock_init(&timer_lock);
    timer_tick = 0;
    crh_init(&crh);
    crh_set_base(&crh, 0);
    return 0;
}

int
timer_sys_init_mp(void)
{
    irq_handler_set(IRQ_TIMER, 0, __tick_handler);
    return 0;
}

static void
timer_process(tick_t c)
{
    /* Assume the IRQ is closed */
    spinlock_acquire(&timer_lock);
    while (c > 0)
    {
        uintptr_t s = crh_max_step(&crh);
        if (s == 0)
        {
            /* XXX error if the return is not NULL */
            crh_set_base(&crh, timer_tick += c);
            break;
        }
          
        if (c < s) s = c;
        
        crh_node_t node = crh_set_base(&crh, timer_tick += s);
        if (node != NULL)
        {
            crh_node_t cur = node;
            while (1)
            {
                timer_t t = CONTAINER_OF(cur, timer_s, node);
                t->in = 0;
                switch (t->type)
                {
                case TIMER_TYPE_WAKEUP:
                    proc_notify(t->wakeup_proc);
                    break;
                }
                
                if ((cur = cur->next) == node) break;
            }
        }
        c -= s;
    }
    spinlock_release(&timer_lock);
}

tick_t
timer_tick_get(void)
{
    tick_t result;
    
    int irq = __irq_save();
    spinlock_acquire(&timer_lock);
    result = timer_tick;
    spinlock_release(&timer_lock);
    __irq_restore(irq);

    return result;
}


static int
__tick_handler(int irq_no, uint64_t acc)
{
    if (PLS(__timer_master_cpu))
        timer_process(acc);

    return 0;
}

void
timer_init(timer_t timer, int type)
{
    timer->in = 0;
    timer->type = type;
}

int
timer_set(timer_t timer, tick_t tick)
{
    int result;
    int irq = irq_save();
    spinlock_acquire(&timer_lock);

    if (timer->in)
        crh_remove(&crh, &timer->node);

    timer->node.key = tick;
    if (crh_insert(&crh, &timer->node) == 0)
    {
        result = 0;
        timer->in = 1;
    }
    else
    {
        result = 1;
        timer->in = 0;
    }
    
    spinlock_release(&timer_lock);
    irq_restore(irq);
    
    return result;
}
