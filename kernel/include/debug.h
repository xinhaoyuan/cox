#ifndef __KERN_DEBUG_H__
#define __KERN_DEBUG_H__

#include <lib/low_io.h>
#include <asm/cpu.h>

#define DBG_IO     (1 << 0)
#define DBG_SCHED  (1 << 1)

#define DEBUG_BITS (DBG_IO | DBG_SCHED)

#define DEBUG_MSG(BITS,INFO) do {               \
        if ((DEBUG_BITS) & (BITS)) {            \
            cprintf INFO ;                      \
        }                                       \
    } while (0)

#define PANIC(x ...) do { cprintf("PANIC: " x); while (1) __cpu_relax(); } while (0)
        
#endif
