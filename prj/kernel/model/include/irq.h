#ifndef __KERN_MODEL_IRQ_H__
#define __KERN_MODEL_IRQ_H__

#include <types.h>

void irq_save(void);
void irq_restore(void);
bool irq_handler(int irq);

#endif
