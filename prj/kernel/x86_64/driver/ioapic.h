#ifndef __KERN_ARCH_IOAPIC_H__
#define __KERN_ARCH_IOAPIC_H__

#include <types.h>

#define IOAPIC_COUNT 256

typedef struct ioapic_s
{
	 int       id;
	 uintptr_t phys;
	 uint32_t  intr_base;
} ioapic_s;

extern int      ioapic_id_set[IOAPIC_COUNT];
extern ioapic_s ioapic[IOAPIC_COUNT];

int  ioapic_init(void);
void ioapic_send_eoi(int apicid, int irq);
void ioapic_enable(int apicid, int irq, int cpunum);
void ioapic_disable(int apicid, int irq);

#endif
