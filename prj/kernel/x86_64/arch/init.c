#include <string.h>

#include <proc.h>

#include <lib/low_io.h>
#include <mm/page.h>
#include <mm/malloc.h>

#include <arch/early_cons.h>
#include <arch/acpi_conf.h>
#include <arch/mem.h>
#include <arch/sysconf_x86.h>
#include <arch/ioapic.h>
#include <arch/lapic.h>
#include <arch/cpu.h>
#include <arch/mp.h>
#include <arch/pic.h>
#include <arch/intr.h>
#include <arch/local.h>
#include <arch/vesa.h>
#include <arch/hpet.h>
#include <arch/init.h>

static proc_s init_proc;
void init(void *);

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
	/* Stage 0: global */
	if (lapic_id() == sysconf_x86.cpu.boot)
	{
		malloc_init();
		sched_init();
		/* Stage 0 ends */
		__cpu_global_init = 1;
	}
	else while (__cpu_global_init == 0) __cpu_relax();
	
	/* Stage 1: processor local */
	/* local data support */
	pls_init();
	/* put self into idle proc */
	sched_init_mp();

	if (lapic_id() == sysconf_x86.cpu.boot)
	{
		uintptr_t stack_top_phys = page_alloc_atomic(4);
		stack_top_phys += PGSIZE * 4;
		
		/* create the init proc */
		proc_init(&init_proc, ".init", SCHED_CLASS_RR, init, NULL, (uintptr_t)VADDR_DIRECT(stack_top_phys));
		proc_notify(&init_proc);

		schedule();
	}

	/* do idle */
	while (1) __cpu_relax();
}

void
init(void *unused)
{
	cprintf("this is init\n");
	while (1) __cpu_relax();
}
