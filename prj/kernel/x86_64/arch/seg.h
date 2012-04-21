#ifndef __KERN_ARCH_SEG_H__
#define __KERN_ARCH_SEG_H__

#include "lapic.h"

/* global segment number */
#define SEG_KTEXT     1
#define SEG_KDATA     2
#define SEG_UTEXT     3
#define SEG_UDATA     4
#define SEG_TSS_BOOT  5
#define SEG_TSS(LCPU) (6 + LAPIC_COUNT * 1 + (LCPU))
#define SEG_PLS(LCPU) (6 + LAPIC_COUNT * 2 + (LCPU))
#define SEG_TLS(LCPU) (6 + LAPIC_COUNT * 3 + (LCPU))
#define SEG_COUNT     (5 + LAPIC_COUNT * 4)

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

#endif
