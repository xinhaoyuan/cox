#include "../pci/pci.h"
#include <runtime/io.h>
#include <lib/low_io.h>
#include <error.h>
#include <string.h>
#include <mach.h>

#define PRIVATE static
#define _PROTOTYPE(name, argslist) name argslist
#define FORWARD
typedef int message;
typedef void sef_init_info_t;

inline static void tickdelay(int ticks) __attribute__((always_inline));
inline static void tickdelay(int ticks) { }

#define pci_attr_r8  pci_conf_read8
#define pci_attr_r16 pci_conf_read16
#define pci_attr_r32 pci_conf_read32
#define pci_attr_w8  pci_conf_write8
#define pci_attr_w16 pci_conf_write16
#define pci_attr_w32 pci_conf_write32

#define OK 0
#define pci_reserve_ok pci_reserve_dev

#define PCI_ILR   0x3c
#define PCI_BAR   0x10
#define PCI_BAR_2 0x18

#define panic(pargs ...) do {                   \
        cprintf(pargs);                         \
        while (1) ;                             \
    } while (0)

#define MAP_FAILED ((void *)-1)
#define vm_map_phys(__IGNORE, base, size) ({                            \
            io_data_s mmio = IO_DATA_INITIALIZER(2, IO_MMIO_OPEN, base, size); \
            io(&mmio, IO_MODE_SYNC);                                    \
            (void *)(mmio.io[0] == 0 ? mmio.io[1] : -1);                \
        })

#define AC_ALIGN4K 0
typedef uintptr_t phys_bytes;

inline static void *alloc_contig(size_t size, size_t align, uintptr_t *phys) __attribute__((always_inline));
inline static
void *alloc_contig(size_t size, size_t align, uintptr_t *phys)
{
    size = (size + __PGSIZE - 1) & ~(__PGSIZE - 1);
    io_data_s phys_alloc = IO_DATA_INITIALIZER(2, IO_PHYS_ALLOC, size);
    io(&phys_alloc, IO_MODE_SYNC);
    if (phys_alloc.io[0]) return NULL;
    *phys = phys_alloc.io[1];
    /* assume success */
    void *result = vm_map_phys(0, *phys, size);
    cprintf("[ALLOC_CONTIG] size: %lx, align: %lx, phys=%p, result=%p\n", size, align, *phys, result);
    return result;
}
