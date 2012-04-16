#include <malloc.h>
#include <error.h>
#include <proc.h>
#include <user.h>
#include <page.h>
#include <mbox.h>
#include <irq.h>
#include <string.h>
#include <mbox_irq.h>
#include <arch/irq.h>
#include <lib/low_io.h>
#include <algo/list.h>
#include <debug.h>

#define USER_THREAD_LIMIT 8192
user_thread_s user_threads[USER_THREAD_LIMIT];
spinlock_s   user_thread_alloc_lock;
list_entry_s user_thread_free_list;

int
user_thread_sys_init(void)
{
    spinlock_init(&user_thread_alloc_lock);
    list_init(&user_thread_free_list);

    int i;
    for (i = 0; i < USER_THREAD_LIMIT; ++ i)
    {
        user_threads[i].proc.type = PROC_TYPE_FREE;
        list_add_before(&user_thread_free_list, &user_threads[i].free_list);
    }

    return 0;
}

static void
user_thread_free(user_thread_t ut)
{
    if (ut == NULL ||
        (ut->proc.type != PROC_TYPE_USER &&
         ut->proc.type != PROC_TYPE_USER_INIT))
        return;

    ut->proc.type = PROC_TYPE_FREE;

    int irq = irq_save();
    spinlock_acquire(&user_thread_alloc_lock);
    
    list_add(&user_thread_free_list, &ut->free_list);    
    
    spinlock_release(&user_thread_alloc_lock);
    irq_restore(irq);
}

int
user_thread_alloc(const char *name, int class, void *stack_base, size_t stack_size)
{
    int irq = irq_save();
    spinlock_acquire(&user_thread_alloc_lock);

    list_entry_t l;
    if (list_empty(&user_thread_free_list))
        l = NULL;
    else
    {
        l = list_next(&user_thread_free_list);
        list_del(l);
    }
    
    spinlock_release(&user_thread_alloc_lock);
    irq_restore(irq);

    if (l == NULL) return -1;

    user_thread_t thread = CONTAINER_OF(l, user_thread_s, free_list);
    proc_init(&thread->proc, name, class, (void(*)(void *))user_thread_jump, NULL, stack_base, stack_size);
    thread->proc.type = PROC_TYPE_USER_INIT;
    /* XXX fill some more data? */
    return thread - user_threads;
}

user_thread_t
user_thread_get(int pid)
{
    /* XXX ref work */
    return &user_threads[pid];
}

void
user_thread_put(user_thread_t ut)
{
    /* XXX ref work */
}


static void do_io_process(proc_t proc, io_call_entry_t entry, iobuf_index_t idx);
static int  user_proc_init(user_proc_t user_proc, uintptr_t *start, uintptr_t *end);
static int  user_thread_init(proc_t proc, uintptr_t entry, uintptr_t tls_u, uintptr_t tls_size, uintptr_t arg0, uintptr_t arg1);
static void print_tls(proc_t proc)
{
    user_thread_t thread = USER_THREAD(proc);
    cprintf("tls          = 0x%016lx tls_u        = 0x%016lx\n",
            thread->tls, thread->tls_u);
    cprintf("io_cap       = 0x%016lx ioce         = 0x%016lx\n",
            thread->io_cap, thread->ioce);
    cprintf("iocr         = 0x%016lx\n",
            thread->iocr);
    cprintf("iocr_khead   = 0x%016lx iocr_utail   = 0x%016lx\n",
            thread->iocr_ctl.khead, thread->iocr_ctl.utail);
    cprintf("iocb         = 0x%016lx\n"
            "iocb_uhead   = 0x%016lx iocb_ktail   = 0x%016lx\n",
            thread->iocb, 
            thread->iocb_ctl.uhead, thread->iocb_ctl.ktail);
}

int
user_thread_init_exec(proc_t proc, void *bin, size_t bin_size)
{
    uintptr_t *ptr = (uintptr_t *)bin;  
    uintptr_t start = *(ptr ++);
    uintptr_t entry = *(ptr ++);
    uintptr_t edata = *(ptr ++);
    uintptr_t end   = *(ptr ++);

    if (proc->type != PROC_TYPE_USER_INIT) return -E_INVAL;
    if (PGOFF(start) || PGOFF(edata) || PGOFF(end)) return -E_INVAL;
    
    user_thread_t thread = USER_THREAD(proc);
    user_proc_t   user_proc;
    if (thread->user_proc != NULL) return -E_INVAL;
    if ((user_proc = kmalloc(sizeof(user_proc_s))) == NULL) return -E_NO_MEM;
    thread->user_proc = user_proc;

    uintptr_t __start = start;
    /* 2 pages for initial tls */
    uintptr_t __end   = end + 2 * PGSIZE;

    int ret;
    if ((ret = user_proc_init(user_proc, &__start, &__end)) != 0) return ret;
    if ((ret = user_thread_init(proc, entry + __start - start,
                                __end - 2 * PGSIZE, 2 * PGSIZE,
                                1, 0)) != 0) return ret;

    uintptr_t now = start;
    while (now < end)
    {
        if (now < edata)
        {
            if ((ret = user_proc_copy_page(user_proc, now + __start - start, 0, 0))) return ret;
        }
        else
        {
            /* XXX: on demand */
            if ((ret = user_proc_copy_page(user_proc, now + __start - start, 0, 0))) return ret;
        }
        now += PGSIZE;
    }
    
    /* copy the binary */
    user_proc_copy(user_proc, __start, bin, bin_size);
    print_tls(proc);
    return 0;
}

int
user_thread_exec(proc_t proc, uintptr_t entry, uintptr_t start, uintptr_t end, uintptr_t arg, proc_t manager)
{
    if (proc->type != PROC_TYPE_USER_INIT) return -E_INVAL;
    if (manager->type != PROC_TYPE_USER) return -E_INVAL;
    if (PGOFF(start) || PGOFF(end)) return -E_INVAL;
    
    user_thread_t thread = USER_THREAD(proc);
    user_proc_t   user_proc;
    
    if (thread->user_proc != NULL) return -E_INVAL;
    if ((user_proc = kmalloc(sizeof(user_proc_s))) == NULL) return -E_NO_MEM;
    thread->user_proc = user_proc;

    uintptr_t __start = start;
    /* 2 pages for initial tls */
    uintptr_t __end   = end + 2 * PGSIZE;

    int ret;
    if ((ret = user_proc_init(user_proc, &__start, &__end)) != 0) return ret;
    if ((ret = user_thread_init(proc, entry + __start - start,
                                __end - 2 * PGSIZE, 2 * PGSIZE,
                                1, arg)) != 0) return ret;

    user_proc->mbox_manage = mbox_get(mbox_alloc(USER_THREAD(manager)->user_proc));
    user_proc->mbox_manage->status = MBOX_STATUS_MANAGE;

    print_tls(proc);

    return 0;
}

int
user_thread_create(proc_t proc, uintptr_t entry, uintptr_t tls_u, size_t tls_size, uintptr_t arg, proc_t from)
{
    if (proc->type != PROC_TYPE_USER_INIT) return -E_INVAL;
    if (from->type != PROC_TYPE_USER) return -E_INVAL;
    if (PGOFF(tls_u) || PGOFF(tls_size) || tls_size < 2 * PGSIZE) return -E_INVAL;

    int ret;
    
    user_thread_t thread = USER_THREAD(proc);
    thread->user_proc = USER_THREAD(from)->user_proc;
    
    if ((ret = user_thread_init(proc, entry, tls_u, tls_size, 0, arg)) != 0) return ret;

    print_tls(proc);
    return 0;
}


int
user_proc_init(user_proc_t user_proc, uintptr_t *start, uintptr_t *end)
{
    int ret;
    if ((ret = user_proc_arch_init(user_proc, start, end))) return ret;
    
    user_proc->mbox_manage = NULL;
        
    user_proc->start = *start;
    user_proc->end =   *end;

    return 0;
}

int
user_thread_init(proc_t proc, uintptr_t entry, uintptr_t tls_u, size_t tls_size, uintptr_t arg0, uintptr_t arg1)
{
    user_thread_t thread = USER_THREAD(proc);
    spinlock_init(&thread->iocb_ctl.push_lock);

    thread->data_size  = PGSIZE;
    thread->iobuf_size = tls_size - PGSIZE;
    thread->tls_u      = tls_u;

    /* touch the memory */
    int i;
    for (i = 0; i < (tls_size >> PGSHIFT); ++ i)
        user_proc_arch_copy_page(thread->user_proc, thread->tls_u + (i << PGSHIFT), 0, 0);
    
    size_t cap = thread->iobuf_size / (sizeof(io_call_entry_s) + sizeof(iobuf_index_t) * 2);
    
    thread->io_cap      = cap;
    /* XXX no mem ? */
    thread->ioce_shadow = kmalloc(sizeof(io_ce_shadow_s) * cap);
    memset(thread->ioce_shadow, 0, sizeof(io_ce_shadow_s) * cap);

    return user_thread_arch_init(proc, entry, arg0, arg1);
}

int
user_proc_copy_page(user_proc_t user_proc, uintptr_t addr, uintptr_t phys, int flag)
{
    return user_proc_arch_copy_page(user_proc, addr, phys, flag);
}

int
user_proc_copy(user_proc_t user_proc, uintptr_t addr, void *src, size_t size)
{
    return user_proc_arch_copy(user_proc, addr, src, size);
}

int
user_proc_brk(user_proc_t user_proc, uintptr_t end)
{
    if (end <= user_proc->start) return -E_INVAL;
    if (end & (PGSIZE - 1)) return -E_INVAL;
    int ret = user_proc_arch_brk(user_proc, end);
    if (ret == 0)
        user_proc->end = end;
    return ret;
}

void
user_thread_jump(void)
{
    __irq_disable();
    current->type = PROC_TYPE_USER;
    user_thread_before_return(current);
    user_thread_arch_jump();
}

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

void
user_thread_before_return(proc_t proc)
{
    /* assume the irq is disabled, ensuring no switch */
    
    if (proc->sched_prev_usr != proc)
    {
        if (proc->sched_prev_usr != NULL)
            user_thread_save_context(proc->sched_prev_usr);
        user_thread_restore_context(proc);
    }
}

int 
user_thread_iocb_push(proc_t proc, iobuf_index_t index)
{
    int irq = irq_save();
    spinlock_acquire(&USER_THREAD(proc)->iocb_ctl.push_lock);

    iobuf_index_t tail = *USER_THREAD(proc)->iocb_ctl.ktail;
    tail %= USER_THREAD(proc)->io_cap;
    USER_THREAD(proc)->iocb[tail] = index;
    if (++ tail == USER_THREAD(proc)->io_cap)
        *USER_THREAD(proc)->iocb_ctl.ktail = 0;
    else *USER_THREAD(proc)->iocb_ctl.ktail = tail;

    spinlock_release(&USER_THREAD(proc)->iocb_ctl.push_lock);
    irq_restore(irq);

    return 0;
}

void
user_thread_save_context(proc_t proc)
{ user_thread_arch_save_context(proc); }

void
user_thread_restore_context(proc_t proc)
{ user_thread_arch_restore_context(proc); }

/* USER IO PROCESS ============================================ */

static inline int do_io_thread_notify(proc_t proc, int pid) __attribute__((always_inline));
static inline int do_io_page_hole_set(proc_t proc, uintptr_t base, uintptr_t size) __attribute__((always_inline));
static inline int do_io_page_hole_clear(proc_t proc, uintptr_t base, uintptr_t size) __attribute__((always_inline));
static inline int do_io_phys_alloc(proc_t proc, size_t size, int flags, uintptr_t *result) __attribute__((always_inline));
static inline int do_io_phys_free(proc_t proc, uintptr_t physaddr) __attribute__((always_inline));
static inline int do_io_mmio_open(proc_t proc, uintptr_t physaddr, size_t size, uintptr_t *result) __attribute__((always_inline));
static inline int do_io_mmio_close(proc_t proc, uintptr_t addr) __attribute__((always_inline));
static inline int do_io_brk(proc_t proc, uintptr_t end) __attribute__((always_inline));
static inline int do_io_sleep(proc_t proc, iobuf_index_t idx, uintptr_t until) __attribute__((always_inline));
static inline int do_io_mbox_open(proc_t proc) __attribute__((always_inline));
static inline int do_io_irq_listen(proc_t proc, int irq_no, int mbox_id) __attribute__((always_inline));
static inline int do_io_mbox_attach(proc_t proc, iobuf_index_t idx, int mbox_id, int to_send, size_t buf_size) __attribute__((always_inline));
static inline int do_io_mbox_detach(proc_t proc, iobuf_index_t idx) __attribute__((always_inline));
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

static inline
int do_io_sleep(proc_t proc, iobuf_index_t idx, uintptr_t until)
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

static inline
int do_io_mbox_attach(proc_t proc, iobuf_index_t idx, int mbox_id, int to_send, size_t buf_size)
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
