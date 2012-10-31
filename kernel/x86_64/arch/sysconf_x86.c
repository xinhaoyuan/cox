#include <string.h>
#include <error.h>
#include <asm/cpu.h>

#include "acpi_conf.h"
#include "sysconf_x86.h"

sysconf_x86_s sysconf_x86;

static void
__detect_cpu(acpi_conf_callbacks_t cb, int apic_id)
{
    int id = sysconf_x86.cpu.count ++;
    sysconf_x86.cpu_id_set[id] = apic_id;
    sysconf_x86.cpus[apic_id].logic_id = id;
}

static void
__detect_lapic(acpi_conf_callbacks_t cb, uintptr_t phys)
{
    sysconf_x86.lapic.enable = TRUE;
    sysconf_x86.lapic.phys   = phys;
}

static void
__detect_ioapic(acpi_conf_callbacks_t cb, int apic_id, uintptr_t phys, int intr_base)
{
    sysconf_x86.ioapic.enable = TRUE;
    
    int id = sysconf_x86.ioapic.count ++;
    sysconf_x86.ioapic_id_set[id]          = apic_id;
    sysconf_x86.ioapics[apic_id].logic_id  = id;
    sysconf_x86.ioapics[apic_id].phys      = phys;
    sysconf_x86.ioapics[apic_id].intr_base = intr_base;
}

static void
__detect_hpet(acpi_conf_callbacks_t cb, uintptr_t phys)
{
    sysconf_x86.hpet.enable = TRUE;
    sysconf_x86.hpet.phys   = phys;
}

int
sysconf_init(void)
{
    struct acpi_conf_callbacks_s cb;
    memset(&cb, 0, sizeof(cb));
    cb.detect_lapic  = __detect_lapic;
    cb.detect_cpu    = __detect_cpu;
    cb.detect_ioapic = __detect_ioapic;
    cb.detect_hpet   = __detect_hpet;

    sysconf_x86.cpu.count = 0;
    sysconf_x86.lapic.enable = FALSE;
    sysconf_x86.ioapic.enable = FALSE;
    sysconf_x86.hpet.enable = FALSE;

    uint32_t b;
    __cpuid(1, NULL, &b, NULL, NULL);
    int cur_apic_id = (b >> 24) & 0xff;
    sysconf_x86.cpu.boot = cur_apic_id;

    return acpi_conf_init(&cb);
}
