#define DEBUG_COMPONENT DBG_MEM

#include <frame.h>
#include <spinlock.h>
#include <lib/buddy.h>
#include <irq.h>
#include <arch/irq.h>

static struct buddy_context_s buddy;
static spinlock_s frame_atomic_lock;

size_t  frames_count;
frame_t frames;

void
frame_sys_early_init_struct(size_t pcount, void*(*init_alloc)(size_t))
{
    frames_count = pcount;
    buddy.node = init_alloc(BUDDY_CALC_ARRAY_SIZE(frames_count) * sizeof(struct buddy_node_s));
    frames = init_alloc(sizeof(frame_s) * frames_count);
    
    spinlock_init(&frame_atomic_lock);
}

void
frame_sys_early_init_layout(int(*layout)(uintptr_t framenum))
{
    buddy_build(&buddy, frames_count, layout);
}

frame_t
frame_alloc(size_t num, int type)
{
    int irq;
    frame_t frame;
    
    irq = irq_save();
    spinlock_acquire(&frame_atomic_lock);
    
    buddy_node_id_t result = buddy_alloc(&buddy, num);

    spinlock_release(&frame_atomic_lock);
    irq_restore(irq);
    
    if (result == BUDDY_NODE_ID_NULL)
        return NULL;
    
    frame = frames + result;
    
    size_t i;
    frame->type      = type;
    frame->ref_count = 1;
    frame->region    = num << 1;
    for (i = 1; i < num; ++ i)
    {
        frame[i].region = ((frame - frames) << 1) | 1;
    }
    
    return frame;
}

static void
frame_free(frame_t frame)
{
    if (frame == NULL || (frame->region & 1))
        PANIC("frame_free on non-head frame!\n");
    
    int irq = irq_save();
    spinlock_acquire(&frame_atomic_lock);
    
    buddy_free(&buddy, frame - frames);
    
    spinlock_release(&frame_atomic_lock);
    irq_restore(irq);
}

void
frame_get(frame_t frame)
{
    if (frame == NULL || frame->region & 1)
        PANIC("frame_get on non-head frame!\n");

    int irq = __irq_save();
    spinlock_acquire(&frame->lock);
    
    ++ frame->ref_count;
    
    spinlock_release(&frame->lock);
    __irq_restore(irq);
}

void
frame_put(frame_t frame)
{
    if (frame == NULL || frame->region & 1)
        PANIC("frame_put on non-head frame!\n");

    int irq = __irq_save();
    spinlock_acquire(&frame->lock);
    
    unsigned int rc = --frame->ref_count;
    
    spinlock_release(&frame->lock);
    __irq_restore(irq);
    
    if (rc == 0)
        frame_free(frame);
}
