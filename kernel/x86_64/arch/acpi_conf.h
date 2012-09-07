#ifndef __KERN_ARCH_ACPI_CONF_H__
#define __KERN_ARCH_ACPI_CONF_H__

/* ACPI module read the system configuration to guide the after
 * initialization. */

#include <types.h>

typedef struct acpi_conf_callbacks_s *acpi_conf_callbacks_t;
/* Callbacks for detecting system layout */
struct acpi_conf_callbacks_s
{
    void  *priv;
    void (*detect_lapic)  (acpi_conf_callbacks_t cb, uintptr_t phys);
    void (*detect_cpu)    (acpi_conf_callbacks_t cb, int apic_id);
    void (*detect_ioapic) (acpi_conf_callbacks_t cb, int apic_id, uintptr_t phys, int intr_base);
    void (*detect_hpet)   (acpi_conf_callbacks_t cb, uintptr_t phys);
};

int acpi_conf_init(acpi_conf_callbacks_t cb);

#endif
