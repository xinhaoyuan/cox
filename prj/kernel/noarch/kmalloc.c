#include <string.h>
#include <frame.h>
#include <kmalloc.h>
#include <ips.h>
#include <lib/sslab.h>
#include <irq.h>
#include <spinlock.h>

struct sslab_ctrl_s sslab_ctrl;

static int        cs_saved_irq[SSLAB_CLASS_COUNT];
static spinlock_s cs_lock[SSLAB_CLASS_COUNT];

static void *
__sslab_page_alloc(struct sslab_ctrl_s *ctrl, unsigned int class, unsigned int flags)
{
    void *result = kframe_open(1, flags);
    return result;
}

static void
__sslab_page_free(struct sslab_ctrl_s *ctrl, unsigned int class, void *page)
{
    kframe_close(page);
}

static void
__sslab_critical_section_enter(struct sslab_ctrl_s *ctrl, unsigned int class)
{
    int irq = irq_save();
    spinlock_acquire(&cs_lock[class]);
    cs_saved_irq[class] = irq;
}

static void
__sslab_critical_section_leave(struct sslab_ctrl_s *ctrl, unsigned int class)
{
    int irq = cs_saved_irq[class];
    spinlock_release(&cs_lock[class]);
    irq_restore(irq);
}

int
kmalloc_sys_init(void)
{
    int result;
    
    sslab_ctrl.page_alloc = __sslab_page_alloc;
    sslab_ctrl.page_free  = __sslab_page_free;
    sslab_ctrl.critical_section_enter = __sslab_critical_section_enter;
    sslab_ctrl.critical_section_leave = __sslab_critical_section_leave;
    
    if ((result = sslab_init(&sslab_ctrl)) < 0) return result;
    int i;
    for (i = 0; i < SSLAB_CLASS_COUNT; ++ i)
        spinlock_init(&cs_lock[i]);

    return 0;
}

void *
kmalloc(size_t size, unsigned int flags)
{
    if (size == 0) return NULL;
    if (size < 8) size = 8;

    void *result;
    
    if (size + SSLAB_META_SIZE > SSLAB_MAX_ALLOC)
    {
        size_t pagesize = (size + _MACH_PAGE_SIZE - 1) >> _MACH_PAGE_SHIFT;
        result = kframe_open(pagesize, flags);
    }
    else
    {
        result = sslab_alloc(&sslab_ctrl, size, flags);
    }

    return result;
}

void
kfree(void *ptr)
{
    if (ptr == NULL) return;
    
    if (((uintptr_t)ptr & (_MACH_PAGE_SIZE - 1)) == 0)
    {
        kframe_close(ptr);
    }
    else
    {
        sslab_free(ptr);
    }
}
