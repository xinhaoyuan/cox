#ifndef __KERN_ARCH_IOAPIC_H__
#define __KERN_ARCH_IOAPIC_H__

#include <types.h>

#define IOAPIC_COUNT 256

int  ioapic_init(void);
void ioapic_send_eoi(int apicid, int irq);
void ioapic_enable(int apicid, int irq, int cpunum);
void ioapic_disable(int apicid, int irq);

#endif
