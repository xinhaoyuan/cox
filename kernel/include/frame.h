#ifndef __KERN_FRAME_H__
#define __KERN_FRAME_H__

#include <types.h>
#include <spinlock.h>
#include <arch/frame.h>

typedef struct frame_s *frame_t;
typedef struct frame_s
{
    spinlock_s   lock;
    uintptr_t    ref_count;
    /* For frame pools */
    frame_t      pool_last;
    
    /* REGION HEAD: (__SIZE__)0 */
    /* REGION BODY: (__HEAD__)1 */
    /* UNMANAGED  : 0 */
    uintptr_t    region;

    frame_arch_s arch;
} frame_s;

extern size_t  frames_count;
extern frame_t frames;

#define FRAME_REGION_SINGLE (2)
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

void frame_sys_early_init_struct(size_t pcount, void*(*init_alloc)(size_t));
void frame_sys_early_init_layout(int(*layout)(uintptr_t frame_num));

frame_t frame_alloc(size_t num);
void    frame_get(frame_t frame);
void    frame_put(frame_t frame);

/* Implemented by ARCH */

/* allocate frames and return the mapping for kernel access */
void *frame_arch_kopen(size_t num);
void  frame_arch_kclose(void *head);

/* open memory access for kernel mmio */
volatile void *mmio_arch_kopen(uintptr_t addr, size_t size);
void           mmio_arch_kclose(volatile void *addr);


#endif
