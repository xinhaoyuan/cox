#include <string.h>
#include <user/arch/iocb.h>
#include <user/arch/syscall.h>
#include <runtime/io.h>
#include <runtime/fiber.h>

inline static iobuf_index_t ioce_alloc_unsafe(void);
inline static void ioce_advance_unsafe(iobuf_index_t idx);
inline static void ioce_free_unsafe(iobuf_index_t idx);

void
do_idle(void)
{
    io_data_t iod;
    fiber_t fiber;
    io_call_entry_t  ce = __ioce;
    iobuf_index_t   idx;
    iobuf_index_t *iocb = __iocb;
    iobuf_index_t  head = __iocb_uhead;
    iobuf_index_t   cap = __io_cap;
    upriv_t           p = __upriv;

    while (1)
    {
        iobuf_index_t tail = __iocb_ktail;

        if (head != tail)
        {
            while (head != tail)
            {
                idx = iocb[head];
                iod = (io_data_t)p->io_shadow[idx];

                if (iod)
                {
                    if (iod->io[0] == IO_MBOX_IO)
                        ce[idx].status = IO_CALL_STATUS_USED;
                    else memmove(iod->io, ce[idx].data, sizeof(uintptr_t) * iod->retc);
                    
                    fiber = (fiber_t)IO_DATA_PTR(iod);
                    IO_DATA_WAIT_CLEAR(iod);
                    if (fiber != NULL) fiber_notify(fiber);
                }
                
                if (ce[idx].status != IO_CALL_STATUS_USED)
                {
                    ioce_free_unsafe(idx);
                    semaphore_release(&p->io_sem);
                }
                
                if (++head == cap) head = 0;
            }
        
            __iocb_uhead_set(head);
        }
        else fiber_schedule();
    }
}

inline static void
io_spin_wait(void)
{
    while (__iocb_uhead == __iocb_ktail)
    {
        // __tick_into_kernel;
    }
}

inline static iobuf_index_t
ioce_alloc_unsafe(void)
{
    io_call_entry_t ce = __ioce;
    iobuf_index_t  idx = __ioce_free;

    if (idx != IOBUF_INDEX_NULL)
    {
        iobuf_index_t next = ce[idx].data[IO_ARG_FREE_NEXT];
        iobuf_index_t prev = ce[idx].data[IO_ARG_FREE_PREV];
        /* cycle dlist remove */
        ce[next].data[IO_ARG_FREE_PREV] = prev;
        ce[prev].data[IO_ARG_FREE_NEXT] = next;

        __ioce_free_set(next == idx ? IOBUF_INDEX_NULL : next);
        ce[idx].status = IO_CALL_STATUS_USED;
    }
    
    return idx;
}

inline static void
ioce_advance_unsafe(iobuf_index_t idx)
{
    __ioce[idx].status = IO_CALL_STATUS_WAIT;
    
    iobuf_index_t tail = __iocr_utail;
    __iocr[tail ++] = idx;
    __iocr_utail_set(tail == __io_cap ? 0 : tail);
}

inline static void
ioce_free_unsafe(iobuf_index_t idx)
{
    io_call_entry_t ce = __ioce;
    ce[idx].status = IO_CALL_STATUS_FREE;
    
    if (__ioce_free == IOBUF_INDEX_NULL)
    {
        ce[idx].data[IO_ARG_FREE_NEXT] =
            ce[idx].data[IO_ARG_FREE_PREV] = idx;
        __ioce_free_set(idx);
    }
    else
    {
        iobuf_index_t prev = __ioce_free;
        iobuf_index_t next = ce[prev].data[IO_ARG_FREE_NEXT];
        
        /* cycle dlist insert */
        ce[idx].data[IO_ARG_FREE_NEXT] = next;
        ce[idx].data[IO_ARG_FREE_PREV] = prev;
        ce[prev].data[IO_ARG_FREE_NEXT] = idx;
        ce[next].data[IO_ARG_FREE_PREV] = idx;
    }
}

void
io_init(void)
{
    io_call_entry_t ce = __ioce;
    int cap = __io_cap;
    iobuf_index_t i;
    for (i = 0; i < cap; ++ i)
    {
        ce[i].status = IO_CALL_STATUS_FREE;
        ce[i].data[IO_ARG_FREE_PREV] = (i + cap - 1) % cap;
        ce[i].data[IO_ARG_FREE_NEXT] = (i + 1) % cap;
    }

    __ioce_free_set(0);
    
    semaphore_init(&__upriv->io_sem, cap);
}

int
io(io_data_t iod, int mode)
{
    ips_node_s __node;
    upriv_t p = __upriv;
    if (semaphore_acquire(&p->io_sem, &__node) == 0)
    {
        ips_wait(&__node);
    }
    
    iobuf_index_t  idx = ioce_alloc_unsafe();
    io_call_entry_t ce = __ioce + idx;

    memmove(ce->data, iod->io, sizeof(uintptr_t) * iod->argc);
    if (mode != IO_MODE_NO_RET)
    {
        p->io_shadow[idx] = iod;
        IO_DATA_WAIT_SET(iod);
        IO_DATA_PTR_SET(iod, __current_fiber);
    }
    else p->io_shadow[idx] = NULL;
    ioce_advance_unsafe(idx);

    if (mode == IO_MODE_SYNC)
    {
        while (IO_DATA_WAIT(iod))
            fiber_wait_try();
    }
    
    return 0;
}

io_call_entry_t
mbox_io_begin(io_data_t iod)
{
    ips_node_s __node;
    upriv_t p = __upriv;
    if (semaphore_acquire(&p->io_sem, &__node) == 0)
    {
        ips_wait(&__node);
    }
    iod->index = ioce_alloc_unsafe();
    iod->io[0] = IO_MBOX_IO;
    iod->io[1] = (uintptr_t)(__ioce + iod->index);
    p->io_shadow[iod->index] = iod;
    return __ioce + iod->index;
}

void *
mbox_io_attach(io_data_t iod, int mbox, int to_send, size_t buf_size)
{
    io_call_entry_t ce = (io_call_entry_t)(iod->io[1]);
    ce->data[0] = IO_MBOX_ATTACH;
    ce->data[1] = mbox;
    ce->data[2] = to_send;
    ce->data[3] = buf_size;

    IO_DATA_WAIT_SET(iod);
    IO_DATA_PTR_SET(iod, __current_fiber);
    
    ioce_advance_unsafe(iod->index);

    while (IO_DATA_WAIT(iod))
        fiber_wait_try();

    if (ce->data[0]) return NULL;
    return (void *)ce->data[1];
}

void
mbox_do_io(io_data_t iod, int mode)
{
    ((io_call_entry_t)(iod->io[1]))->data[0] = IO_MBOX_IO;
    IO_DATA_WAIT_SET(iod);
    IO_DATA_PTR_SET(iod, __current_fiber);
    
    ioce_advance_unsafe(iod->index);

    if (mode == IO_MODE_SYNC)
    {
        while (IO_DATA_WAIT(iod))
            fiber_wait_try();
    }
}

void
mbox_io_end(io_data_t iod)
{
    io_call_entry_t ce = ((io_call_entry_t)(iod->io[1]));

    /* fill the ioce data */
    ce->data[0] = IO_MBOX_DETACH;
    ce->status  = IO_CALL_STATUS_WAIT;
    __upriv->io_shadow[iod->index] = 0;
    
    ioce_advance_unsafe(iod->index);
}

uintptr_t
get_tick(void)
{
    return __get_tick();
}
