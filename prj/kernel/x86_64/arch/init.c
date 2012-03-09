#include <string.h>
#include <irq.h>
#include <timer.h>
#include <proc.h>
#include <entry.h>
#include <page.h>
#include <malloc.h>
#include <arch/local.h>
#include <lib/low_io.h>

#include "early_cons.h"
#include "acpi_conf.h"
#include "mem.h"
#include "sysconf_x86.h"
#include "ioapic.h"
#include "lapic.h"
#include "cpu.h"
#include "mp.h"
#include "pic.h"
#include "intr.h"
#include "vesa.h"
#include "hpet.h"
#include "init.h"

static proc_s init_proc;

/* GRUB info filled by entry32.S */
uint32_t mb_magic;
uint32_t mb_info_phys;

void
__kern_early_init(void) {
	/* Here we jump into C world */

	/* Initialize zero zone */
    extern char __bss[], __end[];
    memset(__bss, 0, __end - __bss);

	early_cons_init();
	mem_init();
	acpi_conf_init();
	idt_init();
	pic_init();
	
	if (!sysconf_x86.ioapic.enable ||
		!sysconf_x86.lapic.enable)
	{
		cprintf("PANIC: ioapic and lapic must be enabled in this arch\n");
		goto err;
	}

	lapic_init();
	ioapic_init();
	if (sysconf_x86.hpet.enable)
		hpet_init();

	if (sysconf_x86.cpu.count > 1)
		mp_init();

	/* all cpus jump to __kern_cpu_init */
	cpu_init();

  err:
	cprintf("You should not be here. Spin now.\n");
	while (1) ;
}

volatile int __cpu_global_init = 0;

void
__kern_cpu_init(void)
{
	/* local data support */
	pls_init();

	/* Stage 0: global */
	if (lapic_id() == sysconf_x86.cpu.boot)
	{
		malloc_init();
		irq_init();
		sched_init();
		timer_init();
		timer_master_cpu_set();
		/* Stage 0 ends */
		__cpu_global_init = 1;
	}
	else while (__cpu_global_init == 0) __cpu_relax();

	/* Stage 1: local */
	/* irq buffering */
	irq_init_mp();
	/* put self into idle proc */
	sched_init_mp();

	if (lapic_id() == sysconf_x86.cpu.boot)
	{
		uintptr_t stack_top_phys = page_alloc_atomic(4);
		stack_top_phys += PGSIZE * 4;

		/* create the init proc */
		proc_init(&init_proc, ".init", SCHED_CLASS_RR, (void(*)(void *))kernel_start, NULL, (uintptr_t)VADDR_DIRECT(stack_top_phys));
		proc_notify(&init_proc);
	}

	/* XXX: TEMP CODE FOR SETUP U->K TRAP STACK */
	uintptr_t stacktop = page_alloc_atomic(4);
	stacktop += 4 * PGSIZE;
	cpu_set_trap_stacktop((uintptr_t)VADDR_DIRECT(stacktop));

	lapic_timer_set(100);
	__irq_enable();
	
	/* do idle */
	while (1) __cpu_relax();
}
