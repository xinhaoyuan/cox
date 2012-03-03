#include <string.h>

#include <lib/low_io.h>
#include <mm/page.h>
#include <mm/malloc.h>

#include <driver/early_cons.h>
#include <driver/acpi_conf.h>
#include <driver/mem.h>
#include <driver/sysconf_x86.h>
#include <driver/ioapic.h>
#include <driver/lapic.h>
#include <driver/cpu.h>
#include <driver/mp.h>
#include <driver/pic.h>
#include <driver/intr.h>

#include <proc/local.h>

#include <init/init.h>

/* Grub info filled by entry32.S */
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
	if (sysconf_x86.cpu.count > 1)
		mp_init();

	/* all cpus jump to __kern_cpu_init */
	cpu_init();

  err:
	cprintf("You should not be here. Spin now.\n");
	while (1) ;
}

volatile int __cpu_init_stage = 0;

void
__kern_cpu_init(void)
{
	/* Stage 0: global data */
	if (lapic_id() == sysconf_x86.cpu.boot)
	{
		malloc_init();
		/* Stage 0 ends */
		__cpu_init_stage = 1;
	}

	/* Stage 1: processor local data */
	pls_init();

	while (1) ;
}
