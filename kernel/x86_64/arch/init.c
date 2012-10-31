#define DEBUG_COMPONENT DBG_MISC

#include <asm/cpu.h>
#include <string.h>
#include <arch/local.h>
#include <lib/buddy.h>
#include <debug.h>
#include <init.h>

#include "early_cons.h"
#include "mem.h"
#include "sysconf_x86.h"
#include "intr.h"
#include "pic.h"
#include "lapic.h"
#include "ioapic.h"
#include "mp.h"
#include "cpu.h"
#include "hpet.h"

#include "init.h"

/* GRUB info filled by entry32.S */
/* do not put them in bss section */
uint32_t __attribute__((section(".data"))) mb_magic = 0;
uint32_t __attribute__((section(".data"))) mb_info_phys = 0;

void
__kern_early_init(void) {
    /* Here we jump into C world */
    int err = 0;
    
    /* Initialize zero zone */
    extern char __bss[], __end[];
    memset(__bss, 0, __end - __bss);

    early_cons_init();
    buddy_init();
    
    err = mem_init();     if (err) goto err;
    err = sysconf_init(); if (err) goto err;
    err = idt_init();     if (err) goto err;
    err = pic_init();     if (err) goto err;

    if (!sysconf_x86.ioapic.enable ||
        !sysconf_x86.lapic.enable)
    {
        DEBUG("ERROR: ioapic and lapic must be enabled in this arch\n");
        goto err;
    }

    err = lapic_init();  if (err) goto err;
    err = ioapic_init(); if (err) goto err;

    if (sysconf_x86.hpet.enable)
    {
        err = hpet_init(); if (err) goto err;
    }

    if (sysconf_x86.cpu.count > 1)
    {
        err = mp_init(); if (err) goto err;
    }

    /* all cpus jump to __kern_cpu_init */
    cpu_init();

  err:
    PANIC("You should not be here. Spin now.\n");
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
