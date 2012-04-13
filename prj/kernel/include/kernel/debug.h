#ifndef __KERN_DEBUG_H__
#define __KERN_DEBUG_H__

#define DBG_IO   (1 << 0)

#define DEBUG_BITS 0

#define DEBUG(BITS,INFO) do {                   \
        if ((DEBUG_BITS) & (BITS)) {            \
            cprintf INFO ;                      \
        }                                       \
    } while (0)
        
#endif
