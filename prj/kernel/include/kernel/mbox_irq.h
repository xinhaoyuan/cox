#ifndef __KERN_MBOX_IRQ_H__
#define __KERN_MBOX_IRQ_H__

#include <arch/irq.h>

struct mbox_s;

int mbox_irq_init(void);
int mbox_irq_listen(int irq_no, struct mbox_s *mbox);
int mbox_irq_handler(int irq_no, uint64_t acc);

#endif
