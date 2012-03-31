#ifndef __KERN_MBOX_IRQ_H__
#define __KERN_MBOX_IRQ_H__

#include <arch/irq.h>

int mbox_irq_init(void);
int mbox_irq_get(int irq_id);
int mbox_irq_handler(int irq_id, uint64_t acc);

#endif
