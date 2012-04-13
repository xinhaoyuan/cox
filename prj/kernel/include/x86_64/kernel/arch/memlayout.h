#ifndef __KERN_ARCH_MEMLAYOUT_H__
#define __KERN_ARCH_MEMLAYOUT_H__

/* Max number of logical cpu can be supported */
#define NR_LCPU       256

/* global segment number */
#define SEG_KTEXT     1
#define SEG_KDATA     2
#define SEG_UTEXT     3
#define SEG_UDATA     4
#define SEG_TSS_BOOT  5
#define SEG_TSS(LCPU) (6 + NR_LCPU * 1 + (LCPU))
#define SEG_PLS(LCPU) (6 + NR_LCPU * 2 + (LCPU))
#define SEG_TLS(LCPU) (6 + NR_LCPU * 3 + (LCPU))
#define SEG_COUNT     (5 + NR_LCPU * 4)

/* global descrptor numbers */
#define GD_KTEXT     ((SEG_KTEXT) << 4)      // kernel text
#define GD_KDATA     ((SEG_KDATA) << 4)      // kernel data
#define GD_UTEXT     ((SEG_UTEXT) << 4)      // user text
#define GD_UDATA     ((SEG_UDATA) << 4)      // user data
#define GD_TSS_BOOT  ((SEG_TSS_BOOT) << 4)   // user data
#define GD_TSS(LCPU) ((SEG_TSS(LCPU)) << 4)        // task segment selector
#define GD_PLS(LCPU) ((SEG_PLS(LCPU)) << 4)
#define GD_TLS(LCPU) ((SEG_TLS(LCPU)) << 4)

#define DPL_KERNEL  (0)
#define DPL_USER    (3)

#define KERNEL_CS   ((GD_KTEXT) | DPL_KERNEL)
#define KERNEL_DS   ((GD_KDATA) | DPL_KERNEL)
#define USER_CS     ((GD_UTEXT) | DPL_USER)
#define USER_DS     ((GD_UDATA) | DPL_USER)

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
