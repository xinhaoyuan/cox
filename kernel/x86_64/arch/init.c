#define DEBUG_COMPONENT DBG_MISC

#include <string.h>
#include <init.h>
#include <arch/local.h>
#include <lib/low_io.h>
#include <frame.h>
#include <debug.h>

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
#include "hpet.h"
#include "init.h"

/* GRUB info filled by entry32.S */
/* do not put them in bss section */
uint32_t __attribute__((section(".data"))) mb_magic = 0;
uint32_t __attribute__((section(".data"))) mb_info_phys = 0;

void
__kern_early_init(void) {
    /* Here we jump into C world */

    /* Initialize zero zone */
    extern char __bss[], __end[];
    memset(__bss, 0, __end - __bss);

    early_cons_init();
    memory_init();
    sysconf_init();
    idt_init();
    pic_init();
    
    if (!sysconf_x86.ioapic.enable ||
        !sysconf_x86.lapic.enable)
    {
        DEBUG("ERROR: ioapic and lapic must be enabled in this arch\n");
        goto err;
    }

    lapic_init();
    ioapic_init();
    
    /* enable kdb intr for test */
    ioapic_enable(ioapic_id_set[0], IRQ_KBD, sysconf_x86.cpu.boot);
    
    if (sysconf_x86.hpet.enable)
        hpet_init();

    if (sysconf_x86.cpu.count > 1)
        mp_init();

    /* all cpus jump to __kern_cpu_init */
    cpu_init();

  err:
    DEBUG("You should not be here. Spin now.\n");
    while (1) __cpu_relax();
}

volatile int __cpu_global_init = 0;

#define TRAP_STACK_SIZE (_MACH_PAGE_SIZE * 4)

PLS_PREALLOC(char, __trap_stack[TRAP_STACK_SIZE]) __attribute__((aligned(_MACH_PAGE_SIZE)));

void
__kern_cpu_init(void)
{
    /* local data support */
    pls_init();

    /* Stage 0: global */
    if (lapic_id() == sysconf_x86.cpu.boot)
    {
        kern_sys_init_global();
        __cpu_global_init = 1;
    }
    else while (__cpu_global_init == 0) __cpu_relax();

    /* Stage 1: local */
    kern_sys_init_local();

    /* SETUP U->K TRAP STACK */
    cpu_set_trap_stacktop((uintptr_t)ARCH_STACKTOP(PLS_PREALLOC_PTR(__trap_stack), TRAP_STACK_SIZE));

    /* All base systems should be ready */
    if (lapic_id() == sysconf_x86.cpu.boot)
    {
        kern_init();
    }

    /* Enable the tick clock */
    lapic_timer_set(100);
    __irq_enable();

    do_idle();
}
