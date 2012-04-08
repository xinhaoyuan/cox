#include <io.h>
#include <mmio.h>
#include <arch/memlayout.h>
#include <lib/low_io.h>

#include "intr.h"
#include "sysconf_x86.h"
#include "ioapic.h"

/* I/O APIC Code from xv6 */

#define REG_ID     0x00  // Register index: ID
#define REG_VER    0x01  // Register index: version
#define REG_TABLE  0x10  // Redirection table base

// The redirection table starts at REG_TABLE and uses
// two registers to configure each interrupt.  
// The first (low) register in a pair contains configuration bits.
// The second (high) register contains a bitmask telling which
// CPUs can serve that interrupt.
#define INT_DISABLED   0x00010000  // Interrupt disabled
#define INT_LEVEL      0x00008000  // Level-triggered (vs edge-)
#define INT_ACTIVELOW  0x00002000  // Active low (vs high)
#define INT_LOGICAL    0x00000800  // Destination is CPU id (vs APIC ID)

// IO APIC MMIO structure: write reg, then read or write data.
struct ioapic_mmio_s
{
    uint32_t reg;
    uint32_t __pad0[3];
    uint32_t data;
    uint32_t __pad1[3];
    uint32_t irq_pin_ass;
    uint32_t __pad2[7];
    uint32_t eoi;
};

typedef volatile struct ioapic_mmio_s ioapic_mmio_s;

static uint32_t
ioapic_read(ioapic_mmio_s *ioapic, uint32_t reg)
{
    ioapic->reg = reg;
    return ioapic->data;
}

static void
ioapic_write(ioapic_mmio_s *ioapic, uint32_t reg, uint32_t data)
{
    ioapic->reg = reg;
    ioapic->data = data;
}

int      ioapic_id_set[IOAPIC_COUNT];
ioapic_s ioapic[IOAPIC_COUNT];

int
ioapic_init(void)
{
    if (!sysconf_x86.ioapic.enable) return 0;
    
    int i, j, id, maxintr;
    int ioapic_id;

    for (i = 0; i != sysconf_x86.ioapic.count; ++ i)
    {
        ioapic_id = ioapic_id_set[i];

        // cprintf("IOAPIC %d phys %016x\n", ioapic_id, ioapic[ioapic_id].phys);
        ioapic[ioapic_id].mmio = mmio_open(ioapic[ioapic_id].phys, sizeof(struct ioapic_mmio_s));

        int v = ioapic_read(ioapic[ioapic_id].mmio, REG_VER);
        maxintr = (v >> 16) & 0xFF;
        cprintf("VERSION: %02x, MAXINTR: %d\n", v & 0xFF, maxintr);
        if ((v & 0xFF) >= 0x20)
            sysconf_x86.ioapic.use_eoi = 1;
        else sysconf_x86.ioapic.use_eoi = 0;
        id = (ioapic_read(ioapic[ioapic_id].mmio, REG_ID) >> 24) & 0xF;
        if (id != ioapic_id)
        {
            cprintf("ioapic_init: id %08x isn't equal to ioapic_id %08x ; Fixing\n",
                    id, ioapic_id);
            ioapic_write(ioapic[ioapic_id].mmio, REG_ID, ioapic_id << 24);
        }

#if 0
        // Mark all interrupts edge-triggered, active high, disabled,
        // and not routed to any CPUs.
        for(j = 0; j <= maxintr; j++)
        {
            ioapic_write(ioapic[ioapic_id].mmio, REG_TABLE + 2 * j, INT_DISABLED | (IRQ_OFFSET + j));
            ioapic_write(ioapic[ioapic_id].mmio, REG_TABLE + 2 * j + 1, 0);
        }
#else
        // Redirect all interrupt to BSP
        for(j = 0; j <= maxintr; j++)
        {
            /* Omit the 2nd IRQ controller? */
            if (0 && j == IRQ_SCTL) {
                ioapic_write(ioapic[ioapic_id].mmio, REG_TABLE + 2 * j, INT_DISABLED | (IRQ_OFFSET + j));
                ioapic_write(ioapic[ioapic_id].mmio, REG_TABLE + 2 * j + 1, 0);
            }
            else
            {
                ioapic_write(ioapic[ioapic_id].mmio, REG_TABLE + 2 * j, (IRQ_OFFSET + j));
                ioapic_write(ioapic[ioapic_id].mmio, REG_TABLE + 2 * j + 1, sysconf_x86.cpu.boot << 24);
            }
        }
#endif
    }

#if 1
    // IMCR:
    // Bochs doesn't support IMCR, so this doesn't run on Bochs.
    // But it would on real hardware.
    __outb(0x22, 0x70);             // Select IMCR
    __outb(0x23, 1);  // Mask external interrupts.
#endif

    return 0;
}

// XXX THIS ROUTINE IS BUGGY
void
ioapic_send_eoi(int apic_id, int irq)
{
    ioapic[apic_id].mmio->eoi = irq & 0xFF;
}

void
ioapic_enable(int apic_id, int irq, int cpunum)
{
    // Mark interrupt edge-triggered, active high,
    // enabled, and routed to the given cpunum,
    // which happens to be that cpu's APIC ID.
    ioapic_write(ioapic[apic_id].mmio, REG_TABLE + 2 * irq, (IRQ_OFFSET + irq));
    ioapic_write(ioapic[apic_id].mmio, REG_TABLE + 2 * irq + 1, cpunum << 24);
}

void
ioapic_disable(int apic_id, int irq)
{
    // Mark interrupt edge-triggered, active high,
    // enabled, and routed to the given cpunum,
    // which happens to be that cpu's APIC ID.
    ioapic_write(ioapic[apic_id].mmio, REG_TABLE + 2 * irq, INT_DISABLED | (IRQ_OFFSET + irq));
    ioapic_write(ioapic[apic_id].mmio, REG_TABLE + 2 * irq + 1, 0);
}
