#include <atom.h>

#include <arch/init.h>
#include <arch/sysconf_x86.h>
#include <arch/intr.h>
#include <arch/mem.h>
#include <arch/cpu.h>

irq_control_s  irq_control[IRQ_COUNT];
unsigned int   cpu_id_set[LAPIC_COUNT];
unsigned int   cpu_id_inv[LAPIC_COUNT];
cpu_static_s   cpu_static[LAPIC_COUNT];
cpu_dynamic_s  cpu_dynamic[LAPIC_COUNT];

static volatile int cpu_init_count = 0;

static struct taskstate ts[NR_LCPU] = {{0}};

void
cpu_init(void)
{
	gdt[SEG_TSS(lapic_id())] = SEGTSS(STS_T32A, (uintptr_t)&ts[lapic_id()], sizeof(ts[lapic_id()]), DPL_KERNEL);
	__ltr(GD_TSS(lapic_id()));
	__lidt(&idt_pd);
		
	int old;
	while (1)
	{
		old = cpu_init_count;
		if (__cmpxchg32(&cpu_init_count, old, old + 1) == old)
			break;
	}

	/* A barrier waiting for all cpu ready */
	while (cpu_init_count != sysconf_x86.cpu.count) ;
	__kern_cpu_init();
}
