#define DEBUG_COMPONENT DBG_IO

#include <string.h>
#include <asm/atom.h>
#include <lib/low_io.h>
#include <debug.h>

#include "init.h"
#include "sysconf_x86.h"
#include "intr.h"
#include "mem.h"
#include "cpu.h"

irq_control_s  irq_control[IRQ_COUNT];
unsigned int   cpu_id_set[LAPIC_COUNT];
unsigned int   cpu_id_inv[LAPIC_COUNT];
cpu_static_s   cpu_static[LAPIC_COUNT];
cpu_dynamic_s  cpu_dynamic[LAPIC_COUNT];

static volatile int cpu_init_count = 0;
static volatile int cpu_boot_pgtab_clean = 0;
static struct taskstate ts[LAPIC_COUNT] = {{0}};

/* Every cpu should fall into this entry after initialization */
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
        __cpu_relax();
    }

    if (old == sysconf_x86.cpu.count - 1)
        DEBUG("ALL CPU STARTED\n");

    /* A barrier that waits for all cpu ready */
    while (cpu_init_count != sysconf_x86.cpu.count) ;

    /* clear the boot pgdir */
    if (lapic_id() == sysconf_x86.cpu.boot)
    {
        memset(pgdir_scratch, 0, PGX(PHYSBASE) * sizeof(pgd_t));
        cpu_boot_pgtab_clean = 1;
    }
    else while (cpu_boot_pgtab_clean == 0) __cpu_relax();

    __lcr3(__rcr3());
    __kern_cpu_init();
}

void
cpu_set_trap_stacktop(uintptr_t stacktop)
{
    ts[lapic_id()].ts_rsp0 = stacktop;
}
