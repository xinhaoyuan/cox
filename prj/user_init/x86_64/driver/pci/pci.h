#ifndef __PCI_H__
#define __PCI_H__

#include <types.h>

// PCI subsystem interface
enum { pci_res_bus, pci_res_mem, pci_res_io, pci_res_max };

struct pci_bus;

struct pci_func {
    struct pci_bus *bus;	// Primary bus for bridges

    uint32_t dev;
    uint32_t func;

    uint32_t dev_id;
    uint32_t dev_class;

    uint32_t reg_base[6];
    uint32_t reg_size[6];
    uint8_t  irq_line;
};

struct pci_bus {
    struct pci_func *parent_bridge;
    uint32_t busno;
};

int pci_init(void);

int pci_first_dev(int *enum_id, uint16_t *vendor, uint16_t *product);
int pci_next_dev(int *enum_id, uint16_t *vendor, uint16_t *product);

uint32_t pci_conf_read32(int enum_id, uint32_t off,  uint32_t *slot);
void     pci_conf_write32(int enum_id, uint32_t off, uint32_t data);

#endif