#include <string.h>
#include <asm/cpu.h>
#include <frame.h>
#include <error.h>
#include <lib/low_io.h>

#include "mem.h"
#include "boot_ap.S.h"
#include "sysconf_x86.h"
#include "cpu.h"
#include "intr.h"
#include "lapic.h"

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
        cpu_static[cpu_id_set[i]].public.idx = i;
        cpu_static[cpu_id_set[i]].public.lapic_id = cpu_id_set[i];
        
        cpu_id_inv[cpu_id_set[i]] = i;
        if (cpu_id_set[i] > max_apic) max_apic = cpu_id_set[i];
    }

    void *boot_ap_stack = kframe_open(max_apic, FA_ATOMIC | FA_KERNEL);
    if (boot_ap_stack == NULL) return -E_NO_MEM;
    
    memset(VADDR_DIRECT(BOOT_AP_STACK_BASE), 0, 8);
    memmove(VADDR_DIRECT(BOOT_AP_STACK_BASE), &boot_ap_stack, sizeof(boot_ap_stack));

    int apic_id;
    for (i = 0; i < sysconf_x86.cpu.count; ++ i)
    {
        apic_id = cpu_id_set[i];
        if (apic_id != sysconf_x86.cpu.boot)
        {
            lapic_ap_start(apic_id, BOOT_AP_ENTRY);
        }
    }
    
    return 0;
}

/* Copy from pmm.c */
static inline void
lgdt(struct pseudodesc *pd) {
    asm volatile ("lgdt (%0)" :: "r" (pd));
    asm volatile ("movw %%ax, %%es" :: "a" (KERNEL_DS));
    asm volatile ("movw %%ax, %%ds" :: "a" (KERNEL_DS));
    // reload cs & ss
    asm volatile (
        "movq %%rsp, %%rax;"            // move %rsp to %rax
        "pushq %1;"                     // push %ss
        "pushq %%rax;"                  // push %rsp
        "pushfq;"                       // push %rflags
        "pushq %0;"                     // push %cs
        "call 1f;"                      // push %rip
        "jmp 2f;"
        "1: iretq; 2:"
        :: "i" (KERNEL_CS), "i" (KERNEL_DS));
}

void
ap_boot_init(void)
{
    lgdt(&gdt_pd);
    lapic_init_ap();
    cpu_init();
}