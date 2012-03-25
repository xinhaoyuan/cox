#include "../pci/pci.h"
#include <runtime/io.h>
#include <lib/low_io.h>

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

#define panic(pargs ...) do {					\
		cprintf(pargs);							\
		while (1) ;								\
	} while (0)

#define MAP_FAILED ((void *)-1)
#define vm_map_phys(__IGNORE, base, size) ({							\
			io_data_s mmio = IO_DATA_INITIALIZER(2, IO_MMIO_OPEN, base, size); \
			io(&mmio, IO_MODE_SYNC);									\
			mmio.io[0] == 0 ? mmio.io[1] : (void *)-1;					\
		})
