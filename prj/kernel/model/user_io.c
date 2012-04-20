#include <user.h>
#include <error.h>
#include <mbox.h>
#include <mbox_irq.h>
#include <page.h>
#include <proc.h>
#include <iosem.h>
#include <malloc.h>

static void do_io_process(proc_t proc, io_call_entry_t entry, iobuf_index_t idx);

void
user_process_io(proc_t proc)
{
    io_call_entry_t ce = USER_THREAD(proc)->ioce;
    iobuf_index_t  cap = USER_THREAD(proc)->io_cap;
    iobuf_index_t head = *USER_THREAD(proc)->iocr_ctl.khead % cap;
    iobuf_index_t tail = *USER_THREAD(proc)->iocr_ctl.utail % cap;
    while (head != tail)
    {
        iobuf_index_t idx = USER_THREAD(proc)->iocr[head];
        if (++ head == cap) head = 0;
        
        if (ce[idx].status == IO_CALL_STATUS_PROC)
            continue;
        
        ce[idx].status = IO_CALL_STATUS_PROC;
        do_io_process(proc, ce + idx, idx);
    }

    *USER_THREAD(proc)->iocr_ctl.khead = head;
}

/* USER IO PROCESS ============================================ */

static inline int  do_io_thread_notify(proc_t proc, int pid) __attribute__((always_inline));
static inline int  do_io_sem_up(proc_t proc, uintptr_t key, size_t num) __attribute__((always_inline));
static inline void do_io_sem_down(proc_t proc, iobuf_index_t idx, uintptr_t key) __attribute__((always_inline));
static inline int  do_io_sem_del(proc_t proc, uintptr_t key) __attribute__((always_inline));
static inline int  do_io_thread_create(proc_t proc, uintptr_t entry, uintptr_t tls_u, size_t tls_size, uintptr_t arg, uintptr_t stack_ptr, uintptr_t *thread_id_slot) __attribute__((always_inline));
static inline int  do_io_page_hole_set(proc_t proc, uintptr_t base, uintptr_t size) __attribute__((always_inline));
static inline int  do_io_page_hole_clear(proc_t proc, uintptr_t base, uintptr_t size) __attribute__((always_inline));
static inline int  do_io_phys_alloc(proc_t proc, size_t size, int flags, uintptr_t *result) __attribute__((always_inline));
static inline int  do_io_phys_free(proc_t proc, uintptr_t physaddr) __attribute__((always_inline));
static inline int  do_io_mmio_open(proc_t proc, uintptr_t physaddr, size_t size, uintptr_t *result) __attribute__((always_inline));
static inline int  do_io_mmio_close(proc_t proc, uintptr_t addr) __attribute__((always_inline));
static inline int  do_io_brk(proc_t proc, uintptr_t end) __attribute__((always_inline));
static inline int  do_io_sleep(proc_t proc, iobuf_index_t idx, uintptr_t until) __attribute__((always_inline));
static inline int  do_io_mbox_open(proc_t proc) __attribute__((always_inline));
static inline int  do_io_irq_listen(proc_t proc, int irq_no, int mbox_id) __attribute__((always_inline));
static inline int  do_io_mbox_attach(proc_t proc, iobuf_index_t idx, int mbox_id, int to_send, size_t buf_size) __attribute__((always_inline));
static inline int  do_io_mbox_detach(proc_t proc, iobuf_index_t idx) __attribute__((always_inline));
static inline void do_io_mbox_io(proc_t proc, iobuf_index_t idx) __attribute__((always_inline));

static void
do_io_process(proc_t proc, io_call_entry_t entry, iobuf_index_t idx)
{
    switch (entry->data[0])
    {
    case IO_THREAD_NOTIFY:
        entry->data[0] = do_io_thread_notify(proc, entry->data[1]);
        user_thread_iocb_push(proc, idx);
        break;

    case IO_SEM_UP:
        entry->data[0] = do_io_sem_up(proc, entry->data[1], entry->data[2]);
        user_thread_iocb_push(proc, idx);
        break;

    case IO_SEM_DOWN:
        do_io_sem_down(proc, idx, entry->data[1]);
        break;
        
    case IO_SEM_DEL:
        entry->data[0] = do_io_sem_del(proc, entry->data[1]);
        user_thread_iocb_push(proc, idx);
        break;

    case IO_THREAD_CREATE:
        entry->data[0] = do_io_thread_create(proc, entry->data[1], entry->data[2], entry->data[3], entry->data[4], entry->data[5], &entry->data[1]);
        user_thread_iocb_push(proc, idx);
        break;
        
    case IO_BRK:
        entry->data[0] = do_io_brk(proc, entry->data[1]);
        user_thread_iocb_push(proc, idx);
        break;

    case IO_SLEEP:
        entry->data[0] = do_io_sleep(proc, idx, entry->data[1]);
        /* IOCB would be pushed when finished */
        break;

    case IO_EXIT:
        /* XXX */
        user_thread_iocb_push(proc, idx);
        break;

    case IO_PAGE_HOLE_SET:
        entry->data[0] = do_io_page_hole_set(proc, entry->data[1], entry->data[2]);
        user_thread_iocb_push(proc, idx);
        break;

    case IO_PAGE_HOLE_CLEAR:
        entry->data[0] = do_io_page_hole_clear(proc, entry->data[1], entry->data[2]);
        user_thread_iocb_push(proc, idx);
        break;

    case IO_DEBUG_PUTCHAR:
        cputchar(entry->data[1]);
        user_thread_iocb_push(proc, idx);
        break;

    case IO_DEBUG_GETCHAR:
        entry->data[1] = cgetchar();
        user_thread_iocb_push(proc, idx);
        break;

    case IO_IRQ_LISTEN:
        entry->data[0] = do_io_irq_listen(proc, entry->data[1], entry->data[2]);
        user_thread_iocb_push(proc, idx);
        break;

    case IO_PHYS_ALLOC:
        entry->data[0] = do_io_phys_alloc(proc, entry->data[1], entry->data[2], &entry->data[1]);
        user_thread_iocb_push(proc, idx);
        break;
        
    case IO_PHYS_FREE:
        entry->data[0] = do_io_phys_free(proc, entry->data[1]);
        user_thread_iocb_push(proc, idx);
        break;
        
    case IO_MMIO_OPEN:
        entry->data[0] = do_io_mmio_open(proc, entry->data[1], entry->data[2], &entry->data[1]);
        user_thread_iocb_push(proc, idx);
        break;

    case IO_MMIO_CLOSE:
        entry->data[0] = do_io_mmio_close(proc, entry->data[1]);
        user_thread_iocb_push(proc, idx);
        break;

    case IO_MBOX_OPEN:
        entry->data[0] = do_io_mbox_open(proc);
        user_thread_iocb_push(proc, idx);
        break;

    case IO_MBOX_CLOSE:
        /* XXX */
        user_thread_iocb_push(proc, idx);
        break;

    case IO_MBOX_ATTACH:
        entry->data[0] = do_io_mbox_attach(proc, idx, entry->data[1], entry->data[2], entry->data[3]);
        user_thread_iocb_push(proc, idx);
        break;

    case IO_MBOX_DETACH:
        entry->data[0] = do_io_mbox_detach(proc, idx);
        user_thread_iocb_push(proc, idx);
        break;

    case IO_MBOX_IO:
        do_io_mbox_io(proc, idx);
        break;

    default: break;
    }
}

static inline int
do_io_thread_notify(proc_t proc, int pid)
{
    user_thread_t thread = user_thread_get(pid);
    if (thread == NULL) return -E_INVAL;
    if (thread->user_proc != USER_THREAD(proc)->user_proc)
    {
        user_thread_put(thread);
        return -E_PERM;
    }
    proc_notify(&thread->proc);
    user_thread_put(thread);
    return 0;
}

static inline int
do_io_sem_up(proc_t proc, uintptr_t key, size_t num)
{
    return iosem_up(&USER_THREAD(proc)->user_proc->iosem_hash, key, num);
}

static inline void
do_io_sem_down(proc_t proc, iobuf_index_t idx, uintptr_t key)
{
    io_ce_shadow_t shd = &USER_THREAD(proc)->ioce_shadow[idx];
    io_call_entry_t ce = &USER_THREAD(proc)->ioce[idx];

    shd->type = IO_CE_SHADOW_TYPE_IOSEM_DOWN;
    int result = iosem_down(&USER_THREAD(proc)->user_proc->iosem_hash, key, &shd->iosem_down_node);
    ce->data[0] = result;

    if (result <= 0)
    {
        shd->type = IO_CE_SHADOW_TYPE_INIT;
        user_thread_iocb_push(proc, idx);
    }
}

static inline int
do_io_sem_del(proc_t proc, uintptr_t key)
{
    return iosem_del(&USER_THREAD(proc)->user_proc->iosem_hash, key);
}

static inline int
do_io_thread_create(proc_t proc, uintptr_t entry, uintptr_t tls_u, size_t tls_size, uintptr_t arg, uintptr_t stack_ptr, uintptr_t *thread_id_slot)
{
    void *stack = kmalloc(__PGSIZE * 4);
    if (stack == NULL) return -E_NO_MEM;
    
    int thread_id = user_thread_alloc(proc->name, SCHED_CLASS_RR, stack, __PGSIZE * 4);
    if (thread_id == -1)
    {
        kfree(stack);
        return -E_NO_MEM;
    }
    
    user_thread_t thread = user_thread_get(thread_id);
    int result = user_thread_create(&thread->proc, entry, tls_u, tls_size, arg, stack_ptr, proc);
    
    if (result)
    {
        user_thread_put(thread);
        kfree(stack);
    }
    else *thread_id_slot = thread_id;

    return result;
}

static inline int
do_io_page_hole_set(proc_t proc, uintptr_t base, uintptr_t size)
{
    /* XXX: Call the arch to set the hole */

    return 0;
}

static inline int
do_io_page_hole_clear(proc_t proc, uintptr_t base, uintptr_t size)
{
    /* XXX */
    return -E_INVAL;
}

static inline int
do_io_phys_alloc(proc_t proc, size_t size, int flags, uintptr_t *result)
{
    if (size == 0 || (size & (PGSIZE - 1))) return -1;
    size >>= PGSHIFT;

    page_t p = page_alloc_atomic(size);
    *result = PAGE_TO_PHYS(p);
    return 0;
}

static inline int
do_io_phys_free(proc_t proc, uintptr_t physaddr)
{
    page_t p = PHYS_TO_PAGE(physaddr);
    if (p == NULL) return -1;

    page_free_atomic(p);

    return 0;
}

static inline int
do_io_mmio_open(proc_t proc, uintptr_t physaddr, size_t size, uintptr_t *result)
{
    int r = user_proc_arch_mmio_open(USER_THREAD(proc)->user_proc, physaddr, size, result);
    return r;
}

static inline int
do_io_mmio_close(proc_t proc, uintptr_t addr)
{
    return user_proc_arch_mmio_close(USER_THREAD(proc)->user_proc, addr);
}

static inline int
do_io_brk(proc_t proc, uintptr_t end)
{
    return user_proc_brk(USER_THREAD(proc)->user_proc, end);
}

static inline int
do_io_sleep(proc_t proc, iobuf_index_t idx, uintptr_t until)
{
    io_ce_shadow_t shd = &USER_THREAD(proc)->ioce_shadow[idx];
    shd->type  = IO_CE_SHADOW_TYPE_TIMER;
    shd->proc  = proc;
    shd->index = idx;
    timer_init(&shd->timer, TIMER_TYPE_USERIO);
    if (timer_set(&shd->timer, until))
    {
        shd->type = IO_CE_SHADOW_TYPE_INIT;
        user_thread_iocb_push(proc, idx);
    }
    return 0;
}

static inline int
do_io_mbox_open(proc_t proc)
{
    return mbox_alloc(USER_THREAD(proc)->user_proc);
}

static inline int
do_io_mbox_attach(proc_t proc, iobuf_index_t idx, int mbox_id, int to_send, size_t buf_size)
{
    mbox_t mbox = mbox_get(mbox_id);
    if (mbox == NULL) return -E_INVAL;
    if (to_send == 0 && mbox->proc != USER_THREAD(proc)->user_proc)
    {
        mbox_put(mbox);
        return -E_PERM;
    }
    
    int r;
    if (to_send)
        r = mbox_user_io_attach(proc, idx, mbox, IO_CE_SHADOW_TYPE_MBOX_SEND_IO, buf_size);
    else r = mbox_user_io_attach(proc, idx, mbox, IO_CE_SHADOW_TYPE_MBOX_RECV_IO, buf_size);

    mbox_put(mbox);
    return r;
}

static inline
int do_io_mbox_detach(proc_t proc, iobuf_index_t idx)
{
    return mbox_user_io_detach(proc, idx);
}

static inline
void do_io_mbox_io(proc_t proc, iobuf_index_t idx)
{
    switch (USER_THREAD(proc)->ioce_shadow[idx].type)
    {
    case IO_CE_SHADOW_TYPE_MBOX_SEND_IO:
        if (mbox_user_send(proc, idx) == 0)
            user_thread_iocb_push(proc, idx);
        break;

    case IO_CE_SHADOW_TYPE_MBOX_RECV_IO:
        if (mbox_user_recv(proc, idx) == 0)
            user_thread_iocb_push(proc, idx);
        break;

    default:
        USER_THREAD(proc)->ioce[idx].data[0] = -E_INVAL;
        user_thread_iocb_push(proc, idx);
        break;
    }
}

static inline int
do_io_irq_listen(proc_t proc, int irq_no, int mbox_id)
{
    mbox_t mbox = mbox_get(mbox_id);
    if (mbox == NULL) return -E_INVAL;
    if (mbox->proc != USER_THREAD(proc)->user_proc)
    {
        mbox_put(mbox);
        return -E_PERM;
    }
    else
    {
        int r = mbox_irq_listen(irq_no, mbox);
        mbox_put(mbox);
        return r;
    }
}
