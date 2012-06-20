#include <lib/sslab.h>
#include <asm/bit.h>
#include <string.h>

int
sslab_init(sslab_ctrl_t ctrl)
{
    int i;
    for (i = 0; i <= SSLAB_ALLOC_DELTA_SHIFT; ++ i)
    {
        ctrl->class_ctrls[i].alloc_size = SSLAB_MIN_ALLOC << i;
        ctrl->class_ctrls[i].index = i;
        ctrl->class_ctrls[i].head  = -1;
    }

    return 0;
}

static void *
sslab_class_alloc(sslab_ctrl_t sslab, unsigned int size_index, unsigned int flags)
{
    sslab_class_ctrl_t class = sslab->class_ctrls + size_index;
    
    sslab->critical_section_enter(sslab, size_index);
    
    if (class->head == (uintptr_t)-1)
    {
        void *block = sslab->page_alloc(sslab, size_index, flags);
         
        if (block == NULL)
        {
            return NULL;
        }
         
        memset(block, 0, _MACH_PAGE_SIZE);
         
        uintptr_t *tail = (uintptr_t *)(
            (uintptr_t)block +
            ((_MACH_PAGE_SIZE / class->alloc_size - 1) * (class->alloc_size)));
        *tail = -1;
         
        class->head = (uintptr_t)block;
    }
     
    uintptr_t *head = (uintptr_t *)class->head;
    class->head = (*head == 0) ? (class->head + class->alloc_size) : (*head);
     
    *head = (uintptr_t)class;

    sslab->critical_section_leave(sslab, size_index);

    return head + 1;
}

void *
sslab_alloc(sslab_ctrl_t sslab, size_t size, uintptr_t flags)
{
    int size_index = BIT_SEARCH_LAST(size + SSLAB_META_SIZE - 1) - SSLAB_MIN_SHIFT + 1;
     
    void *result;
    if (size_index < 0 || size_index > SSLAB_ALLOC_DELTA_SHIFT)
        result = NULL;
    else result = sslab_class_alloc(sslab, size_index, flags);

    return result;
}

void
sslab_free(void *ptr)
{
    if (ptr == NULL) return;
     
    uintptr_t *head = ((uintptr_t *)ptr - 1);
    sslab_class_ctrl_t class_ctrl = (sslab_class_ctrl_t )*head;
    sslab_ctrl_t ctrl = CONTAINER_OF(class_ctrl - class_ctrl->index, sslab_ctrl_s, class_ctrls);

    ctrl->critical_section_enter(ctrl, class_ctrl->index);

    *head = class_ctrl->head;
    class_ctrl->head = (uintptr_t)head;

    ctrl->critical_section_leave(ctrl, class_ctrl->index);
}
