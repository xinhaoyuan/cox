#define DEBUG_COMPONENT DBG_IO

#include <string.h>
#include <error.h>
#include <asm/cpu.h>
#include <asm/mmu.h>
#include <arch/memlayout.h>
#include <debug.h>

#include "acpi_conf.h"

struct acpi_rsdp_s
{
    uint8_t signature[8];   /* ``RSD PTR '' */
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t revision;
    uint32_t rsdt_phys;
    uint32_t rsdt_len;
    uint64_t xsdt_phys;
    uint8_t  xchecksum;
    uint8_t __reserved[3];
} __attribute__((packed));

/* standard table header structure */
struct acpi_sdth_s
{
    uint8_t signature[4];
    uint32_t    length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint64_t    oem_table_id;
    uint32_t    oem_revision;
    uint32_t    creator_id;
    uint32_t    creator_revision;
} __attribute__((packed));

struct acpi_gas_s
{
    uint8_t addr_space_id;
    uint8_t reg_width;
    uint8_t reg_offset;
    uint8_t __reserved;
    union
    {
        uint32_t addr_32;
        uint64_t addr_64;
    };
} __attribute__((packed));

struct acpi_hpet_desc_s
{
    uint32_t block_id;
    struct acpi_gas_s base_low_addr;
    uint8_t  hpet_num;
    uint16_t mc_priod_min_tick;
    uint8_t  pp_oem;
} __attribute__((packed));

struct acpi_madt_s
{
    uint32_t lapic_phys;
    uint32_t flags;
};

struct acpi_madt_apic_desc_s
{
    uint8_t type;
    uint8_t length;
    union
    {
        struct
        {
            uint8_t proc_id;
            uint8_t apic_id;
            uint8_t flags;
        } __attribute__((packed)) lapic;

        struct
        {
            uint8_t apic_id;
            uint8_t reserved;
            uint32_t phys;
            uint32_t intr_base;
        } __attribute__((packed)) ioapic;
    };
} __attribute__((packed));

static uint8_t
sum(uint8_t *addr, int len)
{
    int i, sum;
    sum = 0;
    for(i = 0; i < len; i++)
        sum += addr[i];
    return sum;
}

static struct acpi_rsdp_s*
acpi_rsdp_search_segment(uint8_t *addr, int len, uint64_t align, int ex)
{
    uintptr_t e, p;
    struct acpi_rsdp_s *result = NULL;

    e = (uintptr_t)(addr + len) & ~align;
    p = (uintptr_t)addr;
    if (p & align) p = (p | align) + 1;
    for(; p < e; p = p + align + 1)
    {
        if(memcmp((char *)p, "RSD PTR ", 8) == 0)
        {
            if (sum((unsigned char *)p, ex ? 36 : 20) == 0)
            {
                result = (struct acpi_rsdp_s*)p;
                return result;
            }
        }
    }
    return result;
}

#define BDA 0x400

// Search for the RSDP, which according to the
// spec is in one of the following three locations:
// 1) in the first KB of the EBDA;
// 2) in the BIOS ROM between 0xE0000 and 0xFFFFF.
static struct acpi_rsdp_s*
acpi_rsdp_search(void)
{
    uint8_t *bda;
    uintptr_t p;
    struct acpi_rsdp_s *rsdp;
    uint64_t x;
    /* see ACPI SPEC for details */
    bda = (uint8_t *)VADDR_DIRECT(BDA);
    x = ((bda[0x0F] << 8) | bda[0x0E]) << 4;
    p = (uintptr_t)VADDR_DIRECT(x);
    if (p)
    {
        if ((rsdp = acpi_rsdp_search_segment((uint8_t *)p, 1024, 0xf, 1)))
            return rsdp;
    }
    return acpi_rsdp_search_segment((uint8_t *)VADDR_DIRECT(0xE0000), 0x20000, 0xf, 1);
}

int
acpi_conf_init(acpi_conf_callbacks_t cb)
{
    struct acpi_rsdp_s *rsdp = acpi_rsdp_search();
    if (!rsdp)
    {
        DEBUG("NO RSDP IN MEMORY\n");
        return -E_UNSPECIFIED;
    }

    int xsdp = (rsdp->revision != 0) ? 1 : 0;
    struct acpi_sdth_s *sdt = (struct acpi_sdth_s *)VADDR_DIRECT(xsdp ? rsdp->xsdt_phys : rsdp->rsdt_phys);
    int n = (sdt->length  - sizeof(struct acpi_sdth_s)) / (xsdp ? 8 : 4);

    char sign[5];
    sign[4] = 0;
    int i;
    for (i = 0; i != n; ++ i)
    {
        uintptr_t phys;
        if (xsdp)
            phys = *(uint64_t *)((uintptr_t)(sdt + 1) + (i << (xsdp ? 3 : 2)));
        else phys = *(uint32_t *)((uintptr_t)(sdt + 1) + (i << (xsdp ? 3 : 2)));
        struct acpi_sdth_s *cur = (struct acpi_sdth_s *)VADDR_DIRECT(phys);
        memmove(sign, (char *)&cur->signature, 4);
        DEBUG("ACPI_CONF: processing %s\n", sign);
        if (memcmp(cur->signature, "APIC", 4) == 0)
        {
            struct acpi_madt_s *madt = (struct acpi_madt_s *)(cur + 1);

            if (cb->detect_lapic)
                cb->detect_lapic(cb, madt->lapic_phys);

            char *apic_cur = (char *)(madt + 1);
            char *apic_end = (char *)cur + cur->length;

            while (apic_cur < apic_end)
            {
                struct acpi_madt_apic_desc_s *desc =
                    (struct acpi_madt_apic_desc_s *)apic_cur;

                if (desc->type == 0 && (desc->lapic.flags & 1))
                {
                    /* CPU */
                    if (cb->detect_cpu)
                        cb->detect_cpu(cb, desc->lapic.apic_id);
                }
                else if (desc->type == 1)
                {
                    /* IO APIC */
                    if (cb->detect_ioapic)
                        cb->detect_ioapic(cb, desc->ioapic.apic_id, desc->ioapic.phys, desc->ioapic.intr_base);
                }

                apic_cur += desc->length;
            }
        }
        else if (memcmp(cur->signature, "HPET", 4) == 0)
        {
            struct acpi_hpet_desc_s *hpet = (struct acpi_hpet_desc_s *)(cur + 1);
            if (cb->detect_hpet)
                cb->detect_hpet(cb, hpet->base_low_addr.addr_64);
        }
    }

    return 0;
}
