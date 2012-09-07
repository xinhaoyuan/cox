#ifndef __KERN_ARCH_FRAME_H__
#define __KERN_ARCH_FRAME_H__

#include <types.h>

struct frame_arch_s
{
    int foo;
};

typedef struct frame_arch_s frame_arch_s;
typedef frame_arch_s *frame_arch_t;

#endif
