#ifndef __KERN_IRQ_H__
#define __KERN_IRQ_H__

#include <types.h>

/* Disable the IRQ and return the previous IRQ state */
int  irq_save(void);
/* Restore to the state saved */
void irq_restore(int state);
/* Called by ARCH when got mach-level IRQ */
void irq_entry(int irq);

typedef int(*irq_handler_f)(int irq_no, uint64_t acc);

int irq_sys_init(void);
int irq_sys_init_mp(void);

void irq_handler_set(int irq, int policy, irq_handler_f handler);

#endif
