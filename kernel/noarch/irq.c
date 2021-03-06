#include <asm/bit.h>
#include <string.h>
#include <irq.h>
#include <error.h>
#include <arch/local.h>
#include <arch/irq.h>
#include <proc.h>

PLS_ATOM_DEFINE(int, __local_irq_save, 0);

typedef struct irq_info_s
{
    uint64_t      mask;
    uint64_t      accumulate[IRQ_COUNT];
    irq_handler_f handler[IRQ_COUNT];
    int           policy[IRQ_COUNT];
} irq_info_s;

PLS_PTR_DEFINE(irq_info_s, __irq_info, NULL);

int
irq_sys_init(void)
{
    return 0;
}

PLS_PREALLOC(irq_info_s, __info);

int
irq_sys_init_mp(void)
{
    irq_info_s *info = PLS_PREALLOC_PTR(__info);
    
    PLS_SET(__local_irq_save, 0);
    info->mask = 0;
    memset(info->accumulate, 0, sizeof(info->accumulate));
    memset(info->handler, 0, sizeof(info->handler));
    PLS_SET(__irq_info, info);
    
    return 0;
}

static void
irq_process(void)
{
    int irq_no;
    irq_info_s *info = PLS(__irq_info);
    irq_handler_f h;
    uint64_t acc;
    /* Assume irq is hw-disabled  */
    while (info->mask)
    {
        irq_no = BIT_SEARCH_LAST(info->mask);
        h      = info->handler[irq_no];
        acc    = info->accumulate[irq_no];

        if (h && acc)
        {
            info->accumulate[irq_no] = 0;
            info->mask ^= 1 << irq_no;

            __irq_enable();
            int r = h(irq_no, acc);
            __irq_disable();

            if (r) info->accumulate[irq_no] += acc;
        }
        else
        {
            info->mask ^= 1 << irq_no;
        }
    }
}

int
irq_save(void)
{
    int intr = __irq_save();
    int ret  = PLS(__local_irq_save);
    PLS_SET(__local_irq_save, 1);
    __irq_restore(intr);
    return ret;
}

void
irq_restore(int state)
{
    int intr = __irq_save();    
    struct irq_info_s *info = PLS(__irq_info);
    if (info && state == 0 && info->mask)
    {
        irq_process();
        PLS_SET(__local_irq_save, 0);
    }
    else PLS_SET(__local_irq_save, state);
    __irq_restore(intr);
}

PLS_ATOM_DECLARE(int, __timer_master_cpu);

void
irq_entry(int irq)
{
    int intr = __irq_save();
    irq_info_s *info = PLS(__irq_info);

    info->mask |= (1 << irq);
    ++ info->accumulate[irq];

    if (PLS(__local_irq_save) == 0)
    {
        PLS_SET(__local_irq_save, 1);
        irq_process();
        PLS_SET(__local_irq_save, 0);

        __irq_enable();
        schedule();
    }
    __irq_restore(intr);
}

void
irq_handler_set(int irq_no, int policy, irq_handler_f handler)
{
    int irq = irq_save();
    irq_info_s *info = PLS(__irq_info);
    info->policy[irq_no]  = policy; 
    info->handler[irq_no] = handler;
    irq_restore(irq);
}
