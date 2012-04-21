#ifndef __SLAB_H__
#define __SLAB_H__

#include <mach.h>
#include <types.h>

#define SLAB_META_SIZE sizeof(uintptr_t)
#define SLAB_MIN_ALLOC 8
#define SLAB_MIN_SHIFT 3
#define SLAB_ALLOC_DELTA_SHIFT 10
#define SLAB_MAX_SHIFT (SLAB_MIN_SHIFT + SLAB_ALLOC_DELTA_SHIFT)
#define SLAB_MAX_ALLOC (SLAB_MIN_ALLOC << SLAB_ALLOC_DELTA_SHIFT)
#define SLAB_MIN_BLOCK_PAGE 4
#define SLAB_MAX_BLOCK_PAGE 256

static char __slab_size_assert[(SLAB_MIN_BLOCK_PAGE << _MACH_PAGE_SHIFT) >= SLAB_MAX_ALLOC ? 0 : -1] __attribute__((unused));

struct slab_class_ctrl_s
{
    uint16_t alloc_size;
    uint16_t block_size;
    uintptr_t head;
};

typedef struct slab_class_ctrl_s slab_class_ctrl_s;
typedef slab_class_ctrl_s *slab_class_ctrl_t;

struct slab_ctrl_s
{
    void *(*page_alloc)(size_t num);
    void  (*page_free)(void *addr);
    slab_class_ctrl_s class_ctrls[SLAB_ALLOC_DELTA_SHIFT + 1];
};

typedef struct slab_ctrl_s slab_ctrl_s;
typedef slab_ctrl_s *slab_ctrl_t;

int   slab_init(slab_ctrl_t ctrl);
void *slab_alloc(slab_ctrl_t slab, size_t size);
void  slab_free(void *ptr);

#endif
