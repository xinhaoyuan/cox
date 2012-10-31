#ifndef __KERN_ARCH_SEG_H__
#define __KERN_ARCH_SEG_H__

#include "lapic.h"
#include "seg_boot.h"

#define SEG_TSS(LCPU) (SEG_PER_CPU_START + LAPIC_COUNT * 1 + (LCPU))
#define SEG_PLS(LCPU) (SEG_PER_CPU_START + LAPIC_COUNT * 2 + (LCPU))
#define SEG_TLS(LCPU) (SEG_PER_CPU_START + LAPIC_COUNT * 3 + (LCPU))
#define SEG_COUNT     (SEG_PER_CPU_START + LAPIC_COUNT * 4)

#define GD_TSS(LCPU) ((SEG_TSS(LCPU)) << 4)
#define GD_PLS(LCPU) ((SEG_PLS(LCPU)) << 4)
#define GD_TLS(LCPU) ((SEG_TLS(LCPU)) << 4)

#endif
