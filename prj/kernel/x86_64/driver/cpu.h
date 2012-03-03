#ifndef __KERN_ARCH_CPU_H__
#define __KERN_ARCH_CPU_H__

#include <types.h>

#include <driver/memlayout.h>
#include <driver/lapic.h>
#include <driver/intr.h>

typedef void(*intr_handler_f)(struct trapframe *tf);

typedef struct cpu_info_s
{
	int      lapic_id;
	int      idx;
	uint64_t freq;
} cpu_info_s;

typedef struct cpu_static_s
{
	pgd_t *init_pgdir;
	intr_handler_f intr_handler[256];

	cpu_info_s public;
} cpu_static_s;

typedef struct cpu_dynamic_s
{
} cpu_dynamic_s;

typedef struct irq_control_s
{
	int cpu_apic_id;
} irq_control_s;

extern irq_control_s irq_control[IRQ_COUNT];

extern unsigned int   cpu_id_set[LAPIC_COUNT];
extern unsigned int   cpu_id_inv[LAPIC_COUNT];
/* Indexed by APIC ID */
extern cpu_static_s  cpu_static[LAPIC_COUNT];
extern cpu_dynamic_s cpu_dynamic[LAPIC_COUNT];

void cpu_init(void) __attribute__((noreturn));

#endif
