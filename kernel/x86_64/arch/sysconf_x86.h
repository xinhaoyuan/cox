#ifndef __KERN_ARCH_SYSCONF_X86_H__
#define __KERN_ARCH_SYSCONF_X86_H__

#include <types.h>
#include "lapic.h"
#include "ioapic.h"

/* Define the structure for configuration of arch  */

typedef struct sysconf_x86_s
{
    struct {
        int boot;
        int count;
    } cpu;

    struct {
        bool      enable;
        uintptr_t phys;
    } lapic;

    struct
    {
        bool enable;
        int  count;
        bool use_eoi;
    } ioapic;

    struct
    {
        bool      enable;
        uintptr_t phys;
    } hpet;

    int ioapic_id_set[IOAPIC_COUNT];
    /* INDEXED BY APIC ID */
    struct {
        int       logic_id;
        uintptr_t phys;
        uint32_t  intr_base;
    } ioapics[IOAPIC_COUNT];

    int cpu_id_set[LAPIC_COUNT];
    /* INDEXED BY APIC ID */
    struct
    {
        int      logic_id;
        uint64_t freq;
    } cpus[LAPIC_COUNT];

    
} sysconf_x86_s;

extern sysconf_x86_s sysconf_x86;

int sysconf_init(void);

#endif
