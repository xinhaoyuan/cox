#ifndef __KERN_ARCH_FRAME_H__
#define __KERN_ARCH_FRAME_H__

#include <types.h>
#include <spinlock.h>

typedef struct frame_arch_s *frame_arch_t;
typedef struct frame_arch_s
{
    union
    {
        struct
        {
            struct user_proc_s *up;
            uintptr_t           index;
        } unnamed;

        struct
        {
            void      *inv_map_data;
        } object;
    };
} frame_arch_s;

#define FRAME_ARCH_UNNAMED_LEVEL(index)    ((index) & (_MACH_PAGE_SIZE - 1))
#define FRAME_ARCH_UNNAMED_LA(index)       ((index) & ~(uintptr_t)(_MACH_PAGE_SIZE - 1))
#define FRAME_ARCH_UNNAMED_INDEX(la,level) (FRAME_ARCH_UNNAMED_LA(la) | FRAME_ARCH_UNNAMED_LEVEL(level))

#endif
