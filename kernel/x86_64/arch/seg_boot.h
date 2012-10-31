#ifndef __KERN_ARCH_SEG_BOOT_H__
#define __KERN_ARCH_SEG_BOOT_H__

/* global segment number */
#define SEG_KTEXT         1
#define SEG_KDATA         2
#define SEG_UTEXT         3
#define SEG_UDATA         4
#define SEG_TSS_BOOT      5
#define SEG_PER_CPU_START 6

/* global descrptor numbers */
#define GD_KTEXT     ((SEG_KTEXT) << 4)      // kernel text
#define GD_KDATA     ((SEG_KDATA) << 4)      // kernel data
#define GD_UTEXT     ((SEG_UTEXT) << 4)      // user text
#define GD_UDATA     ((SEG_UDATA) << 4)      // user data
#define GD_TSS_BOOT  ((SEG_TSS_BOOT) << 4)   // user data

#define DPL_KERNEL  (0)
#define DPL_USER    (3)

#define KERNEL_CS   ((GD_KTEXT) | DPL_KERNEL)
#define KERNEL_DS   ((GD_KDATA) | DPL_KERNEL)
#define USER_CS     ((GD_UTEXT) | DPL_USER)
#define USER_DS     ((GD_UDATA) | DPL_USER)

#endif
