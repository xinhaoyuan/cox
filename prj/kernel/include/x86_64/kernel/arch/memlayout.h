#ifndef __KERN_ARCH_MEMLAYOUT_H__
#define __KERN_ARCH_MEMLAYOUT_H__

/* *
 * Virtual memory map:                                          Permissions
 *                                                              kernel/user
 *
 *     4G x 4G -------------> +---------------------------------+
 *                            |                                 |
 *                            |   Remapped Memory for Kernel    | RW/-- KERNSIZE
 *                            |                                 |
 *     KERNBASE ------------> +---------------------------------+ 0xFFFFFFFF80000000
 *                            |                                 |
 *                            |         Invalid Memory (*)      |
 *                            |                                 |
 *                            +---------------------------------+ 0xFFFFD08000000000
 *                            |   Cur. Page Table (Kern, RW)    | RW/-- PUSIZE
 *     VPT -----------------> +---------------------------------+ 0xFFFFD00000000000
 *                            |                                 |
 *                            |  Linear Mapping of Physical Mem | RW/-- PHYSSIZE
 *                            |                                 |
 *              PHYSBASE ---> +---------------------------------+ 0xFFFF900000000000
 *                            |                                 |
 *                            |                                 |
 *                            |                                 |
 *                            ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * (*) Note: The supervisor ensures that "Invalid Memory" is *never* mapped.
 *     "Empty Memory" is normally unmapped, but user programs may map pages
 *     there if desired.
 *
 * */

#define KERNBASE              0xFFFFFFFF80000000
#define KERN_INIT_STACK_SIZE  0x4000

/* All physical memory mapped at this address */
#define PHYSBASE            0xFFFF900000000000
#define PHYSSIZE            0x0000400000000000          // the maximum amount of physical memory

#define VPT                 0xFFFFD00000000000

#define KVBASE              0xFFFFE00000000000
#define KVSIZE              0x0000000080000000

#define UMMIO_BASE          0x0000000010000000
#define UMMIO_SIZE          0x0000000030000000
#define USTART              0x0000000040000000
#define UTOP                0x0000010000000000

#ifndef __ASSEMBLER__

#define ARCH_STACKTOP(base, size) ((void *)((uintptr_t)(base) + (size)))
#define VADDR_DIRECT(paddr) ((void *)((paddr) + PHYSBASE))
#define PADDR_DIRECT(paddr) (((uintptr_t)(paddr)) - PHYSBASE)

#include <types.h>

typedef uintptr_t pgd_t;
typedef uintptr_t pud_t;
typedef uintptr_t pmd_t;
typedef uintptr_t pte_t;

#endif

#endif
