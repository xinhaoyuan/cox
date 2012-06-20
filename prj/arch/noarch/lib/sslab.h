#ifndef __SSLAB_H__
#define __SSLAB_H__

#include <asm/mach.h>
#include <types.h>

#define SSLAB_META_SIZE sizeof(uintptr_t)
#define SSLAB_MIN_ALLOC 8
#define SSLAB_MIN_SHIFT 3
#define SSLAB_MAX_SHIFT (_MACH_PAGE_SHIFT - 1)
#define SSLAB_MAX_ALLOC (_MACH_PAGE_SIZE / 2)
#define SSLAB_ALLOC_DELTA_SHIFT (SSLAB_MAX_SHIFT - SSLAB_MIN_SHIFT)
#define SSLAB_CLASS_COUNT (SSLAB_ALLOC_DELTA_SHIFT + 1)

static char __sslab_size_assert1[SSLAB_ALLOC_DELTA_SHIFT >= 0 ? 0 : -1] __attribute__((unused));
static char __sslab_size_assert2[_MACH_PAGE_SIZE >= SSLAB_MAX_ALLOC ? 0 : -1] __attribute__((unused));

struct sslab_class_ctrl_s
{
    uint16_t  alloc_size;
    uint16_t  index;
    uintptr_t head;
};

typedef struct sslab_class_ctrl_s sslab_class_ctrl_s;
typedef sslab_class_ctrl_s *sslab_class_ctrl_t;

struct sslab_ctrl_s
{
    void  (*critical_section_enter)(struct sslab_ctrl_s *ctrl, unsigned int class);
    void  (*critical_section_leave)(struct sslab_ctrl_s *ctrl, unsigned int class);
    
    void *(*page_alloc)(struct sslab_ctrl_s *ctrl, unsigned int class, unsigned int flags);
    void  (*page_free) (struct sslab_ctrl_s *ctrl, unsigned int class, void *addr);
    
    sslab_class_ctrl_s class_ctrls[SSLAB_CLASS_COUNT];

    void *priv;
};

typedef struct sslab_ctrl_s sslab_ctrl_s;
typedef sslab_ctrl_s *sslab_ctrl_t;

int   sslab_init(sslab_ctrl_t ctrl);
void *sslab_alloc(sslab_ctrl_t sslab, size_t size, uintptr_t flags);
void  sslab_free(void *ptr);

#endif
