#ifndef __KERN_FRAME_H__
#define __KERN_FRAME_H__

#include <types.h>
#include <spinlock.h>
#include <arch/frame.h>

struct frame_s
{
    spinlock_s   lock;
    unsigned int ref_count;
    
    /* REGION HEAD: (__SIZE__)0 */
    /* REGION BODY: (__HEAD__)1 */
    uintptr_t    region;

    frame_arch_s arch;
};

typedef struct frame_s frame_s;
typedef frame_s *frame_t;

extern size_t  frames_count;
extern frame_t frames;

#define FRAME_REGION_HEAD(frame) ({ frame_t p = (frame); (p->region & 1) ? (frames + (p->region >> 1)) : p; })
#define FRAME_REGION_SIZE(frame) ({ frame_t p = (frame); (p->region & 1) ? (frames[(p->region >> 1)]->region >> 1) : (p->region >> 1); })

#ifndef FRAME_ARCH_MAP
#include <asm/mach.h>
#include <debug.h>
#define PHYS_TO_FRAME(addr) ({ size_t idx = (addr) >> _MACH_PAGE_SHIFT; idx < frames_count ? frames + idx : NULL; })
#define FRAME_TO_PHYS(frame) ({ size_t idx = (frame) - frames; \
            if (idx >= frames_count) { PANIC("FRAME_TO_PHYS out of range"); } \
            idx << PGSHIFT; })
#else
/* or the arch defines the translation */
#endif

void frame_sys_init_struct(size_t pcount, void*(*init_alloc)(size_t));
void frame_sys_init_layout(int(*layout)(uintptr_t frame_num));

frame_t frame_alloc(size_t num, unsigned int flags);
void    frame_get(frame_t frame);
void    frame_put(frame_t frame);

#define FA_NO         0x1
#define FA_KERNEL     0x2
#define FA_ATOMIC     0x4
#define FA_CONTIGUOUS 0x8

/* Implemented by ARCH */

/* allocate frames and return the mapping for kernel access */
void *kframe_open(size_t num, unsigned int flags);
void  kframe_close(void *head);

/* open memory access for kernel mmio */
volatile void *kmmio_open(uintptr_t addr, size_t size);
void           kmmio_close(volatile void *addr);


#endif
