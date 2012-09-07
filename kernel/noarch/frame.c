#include <frame.h>
#include <spinlock.h>
#include <irq.h>
#include <lib/buddy.h>
#include <asm/mmu.h>
#include <error.h>
#include <proc.h>
#include <ips.h>

#include <arch/memlayout.h>
#include <arch/local.h>

static struct buddy_context_s buddy;
static spinlock_s frame_atomic_lock;
static spinlock_s frame_alloc_wait_lock;
static ips_node_t frame_alloc_wait_head;

size_t  frames_count;
frame_t frames;

#define FRAME_ALLOC_TRY_COUNT_MAX 16

static void frame_free(frame_t frame);
static void frame_alloc_wakeup_all(void);

void
frame_sys_init_struct(size_t pcount, void*(*init_alloc)(size_t))
{
    buddy_init();
    
    frames_count = pcount;
    buddy.node = init_alloc(BUDDY_CALC_ARRAY_SIZE(frames_count) * sizeof(struct buddy_node_s));
    frames = init_alloc(sizeof(frame_s) * frames_count);
    
    spinlock_init(&frame_atomic_lock);
    spinlock_init(&frame_alloc_wait_lock);
    frame_alloc_wait_head = NULL;
}

void
frame_sys_init_layout(int(*layout)(uintptr_t framenum))
{
    buddy_build(&buddy, frames_count, layout);
}

static void
frame_alloc_wakeup_all(void)
{
    int irq = irq_save();
    spinlock_acquire(&frame_alloc_wait_lock);

    ips_node_t cur = frame_alloc_wait_head;
    while (cur != NULL)
    {
        IPS_NODE_WAIT_CLEAR(cur);
        proc_notify((proc_t)IPS_NODE_PTR(cur));
        if ((cur = cur->next) == frame_alloc_wait_head) break;
    }

    frame_alloc_wait_head = NULL;
    
    spinlock_release(&frame_alloc_wait_lock);
    irq_restore(irq);
}

frame_t
frame_alloc(size_t num, unsigned int flags)
{
    int irq;
    frame_t frame;
    unsigned int try_count = 0;
    
  try_again:
    
    irq = irq_save();
    spinlock_acquire(&frame_atomic_lock);
    
    buddy_node_id_t result = buddy_alloc(&buddy, num);

    spinlock_release(&frame_atomic_lock);
    irq_restore(irq);
    
    if (result == BUDDY_NODE_ID_NULL &&
        !(flags & FA_ATOMIC) &&
        try_count < FRAME_ALLOC_TRY_COUNT_MAX)
    {
        ++ try_count;
        goto try_again;
    }

    if (result == BUDDY_NODE_ID_NULL)
        return NULL;
    
    frame = frames + result;
    
    size_t i;
    frame->region = num << 1;
    for (i = 1; i < num; ++ i)
    {
        frame[i].region = ((frame - frames) << 1) | 1;
    }
    
    return frame;
}

static void
frame_free(frame_t frame)
{
    if (frame == NULL ||frame->region & 1)
        PANIC("frame_free on non-head frame!\n");
    
    int irq = irq_save();
    spinlock_acquire(&frame_atomic_lock);
    
    buddy_free(&buddy, frame - frames);
    
    spinlock_release(&frame_atomic_lock);
    irq_restore(irq);

    frame_alloc_wakeup_all();
}

void
frame_get(frame_t frame)
{
    if (frame == NULL || frame->region & 1)
        PANIC("frame_get on non-head frame!\n");

    int irq = irq_save();
    spinlock_acquire(&frame->lock);

    ++frame->ref_count;
    
    spinlock_release(&frame->lock);
    irq_restore(irq);
}

void
frame_put(frame_t frame)
{
    if (frame == NULL || frame->region & 1)
        PANIC("frame_put on non-head frame!\n");

    int irq = irq_save();
    spinlock_acquire(&frame->lock);

    unsigned int rc = --frame->ref_count;
    
    spinlock_release(&frame->lock);
    irq_restore(irq);

    if (rc == 0) frame_free(frame);
}
