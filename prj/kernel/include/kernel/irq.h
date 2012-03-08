#ifndef __KERN_IRQ_H__
#define __KERN_IRQ_H__

#include <types.h>

int  irq_save(void);
void irq_restore(int state);
void irq_entry(int irq);

typedef void(*irq_handler_f)(int irq_no, uint64_t acc);
extern irq_handler_f irq_handler[];

int irq_init(void);
int irq_init_mp(void);

#endif
