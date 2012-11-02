#include <string.h>
#include <asm/cpu.h>
#include <frame.h>
#include <error.h>
#include <arch/memlayout.h>

#include "mem.h"
#include "boot_ap.S.h"
#include "sysconf_x86.h"
#include "intr.h"
#include "lapic.h"
#include "cpu.h"

/* Provided by link script */
extern char boot_ap_entry_64[];
extern char boot_ap_entry_64_end[];

int
mp_init(void)
{
    memset(VADDR_DIRECT(BOOT_AP_ENTRY), 0, PGSIZE);
    memmove(VADDR_DIRECT(BOOT_AP_ENTRY), boot_ap_entry_64, boot_ap_entry_64_end - boot_ap_entry_64);

    memset(VADDR_DIRECT(BOOT_AP_CR3), 0, 8);
    uintptr_t cr3 = __rcr3();
    memmove(VADDR_DIRECT(BOOT_AP_CR3), &cr3, sizeof(cr3));

    memset(VADDR_DIRECT(BOOT_AP_LAPIC_PHYS), 0, 8);
    memmove(VADDR_DIRECT(BOOT_AP_LAPIC_PHYS), &sysconf_x86.lapic.phys, sizeof(sysconf_x86.lapic.phys));

    int i, max_apic = 0;
    for (i = 0; i != sysconf_x86.cpu.count; ++ i)
    {
        int apic_id = sysconf_x86.cpu_id_set[i];
        sysconf_x86.cpus[apic_id].logic_id = i;
        if (apic_id > max_apic) max_apic = apic_id;
    }

    void *boot_ap_stack = frame_arch_kopen(max_apic + 1);
    
    if (boot_ap_stack == NULL) return -E_NO_MEM;

    boot_ap_stack = ARCH_STACKTOP(boot_ap_stack, _MACH_PAGE_SIZE);
    
    memset(VADDR_DIRECT(BOOT_AP_STACK_BASE), 0, sizeof(boot_ap_stack));
    memmove(VADDR_DIRECT(BOOT_AP_STACK_BASE), &boot_ap_stack, sizeof(boot_ap_stack));

    int apic_id;
    for (i = 0; i < sysconf_x86.cpu.count; ++ i)
    {
        apic_id = sysconf_x86.cpu_id_set[i];
        if (apic_id != sysconf_x86.cpu.boot)
        {
            lapic_ap_start(apic_id, BOOT_AP_ENTRY);
        }
    }
    
    return 0;
}

void
ap_boot_init(void)
{
    lgdt(&gdt_pd);
    lapic_init_ap();
    cpu_init();
}
