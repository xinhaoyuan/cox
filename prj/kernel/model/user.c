#include <malloc.h>
#include <error.h>
#include <proc.h>
#include <user.h>
#include <page.h>
#include <mbox.h>
#include <irq.h>
#include <arch/irq.h>
#include <lib/low_io.h>

static void io_process(proc_t proc, io_call_entry_t entry, iobuf_index_t idx);
static int user_proc_init(user_proc_t mm, uintptr_t *start, uintptr_t *end);
static int user_thread_init(proc_t proc, uintptr_t entry);
static void print_tls(proc_t proc)
{		
	cprintf("tls        = 0x%016lx tls_u        = 0x%016lx\n",
			proc->user_thread->tls, proc->user_thread->tls_u);
	cprintf("iocb_busy  = 0x%016lx iocb_cap     = %d\n"
			"iocb_head  = 0x%016lx iocb_tail    = 0x%016lx\n",
			proc->user_thread->iocb.busy, proc->user_thread->iocb.cap,
			proc->user_thread->iocb.head, proc->user_thread->iocb.tail);
	cprintf("iocb_entry = 0x%016lx ioce_head    = 0x%016lx\n",
			proc->user_thread->iocb.entry, proc->user_thread->ioce.head);
	cprintf("iocb_stack = 0x%016lx iocb_stack_u = 0x%016lx\n",
			proc->user_thread->iocb.stack, proc->user_thread->iocb.stack_u);
}

int
user_proc_load(void *bin, size_t bin_size)
{
	uintptr_t *ptr = (uintptr_t *)bin;	
	uintptr_t start = *(ptr ++);
	uintptr_t entry = *(ptr ++);
	uintptr_t edata = *(ptr ++);
	uintptr_t end   = *(ptr ++);

	if (PGOFF(start) || PGOFF(edata) || PGOFF(end)) return -E_INVAL;

	proc_t proc = current;

	if ((proc->user_proc = kmalloc(sizeof(user_proc_s))) == NULL) return -E_NO_MEM;
	if ((proc->user_thread = kmalloc(sizeof(user_thread_s))) == NULL) return -E_NO_MEM;

	uintptr_t __start = start;
	/* 3 pages for initial tls */
	uintptr_t __end   = end + 3 * PGSIZE;

	int ret;
	if ((ret = user_proc_init(proc->user_proc, &__start, &__end)) != 0) return ret;
	if ((ret = user_thread_init(proc, entry + __start - start)) != 0) return ret;

	uintptr_t now = start;
	while (now < end)
	{
		if (now < edata)
		{
			if ((ret = user_proc_copy_page(proc->user_proc, now + __start - start, 0, 0))) return ret;
		}
		else
		{
			/* XXX: on demand */
			if ((ret = user_proc_copy_page(proc->user_proc, now + __start - start, 0, 0))) return ret;
		}
		now += PGSIZE;
	}
	
	/* copy the binary */
	user_proc_copy(proc->user_proc, __start, bin, bin_size);
	print_tls(proc);
	return 0;
}

int
user_proc_init(user_proc_t mm, uintptr_t *start, uintptr_t *end)
{
	int ret;
	if ((ret = user_proc_arch_init(mm, start, end))) return ret;

	mm->start = *start;
	mm->end =   *end;

	return 0;
}

int
user_thread_init(proc_t proc, uintptr_t entry)
{
	user_thread_t t = proc->user_thread;
	spinlock_init(&proc->user_thread->iocb.push_lock);

	proc->user_thread->ph_guard = IOBUF_INDEX_NULL;
	
	t->user_size = PGSIZE;
	t->iobuf_size = PGSIZE;
	t->iocb_stack_size = PGSIZE;
	t->tls_u = proc->user_proc->end - 3 * PGSIZE;

	/* touch the memory */
	user_proc_arch_copy_page(proc->user_proc, t->tls_u, 0, 0);
   	user_proc_arch_copy_page(proc->user_proc, t->tls_u + PGSIZE, 0, 0);
	user_proc_arch_copy_page(proc->user_proc, t->tls_u + PGSIZE * 2, 0, 0);
	
	t->iocb.stack_u = t->tls_u + t->user_size;
	proc->user_thread->iocb.callback_u = 0;

	size_t cap = t->iobuf_size / (sizeof(io_call_entry_s) + sizeof(iobuf_index_t));
	
	proc->user_thread->iocb.cap = cap;
	proc->user_thread->ioce.cap = cap;

	return user_thread_arch_init(proc, entry);
}

int
user_proc_copy_page(user_proc_t mm, uintptr_t addr, uintptr_t phys, int flag)
{
	return user_proc_arch_copy_page(mm, addr, phys, flag);
}

int
user_proc_copy(user_proc_t mm, uintptr_t addr, void *src, size_t size)
{
	return user_proc_arch_copy(mm, addr, src, size);
}

int
user_proc_brk(user_proc_t mm, uintptr_t end)
{
	cprintf("BRK: %016lx\n", end);
	
	if (end <= mm->start) return -E_INVAL;
	if (end & (PGSIZE - 1)) return -E_INVAL;
	int ret = user_proc_arch_brk(mm, end);
	if (ret == 0)
		mm->end = end;
	return ret;
}

void
user_thread_jump(void)
{
	__irq_disable();
	
	proc_t proc = current;
	proc->type = PROC_TYPE_USER;
	user_before_return(proc);
	user_thread_arch_jump();
}

void
user_process_io(proc_t proc)
{
	io_call_entry_t head = proc->user_thread->ioce.head;
	while (head->head.head != 0 &&
		   head->head.head != head->head.tail)
	{
		head->head.head %= proc->user_thread->ioce.cap;

		if (head[head->head.head].ce.status == IO_CALL_STATUS_PROC) break;
		head[head->head.head].ce.status = IO_CALL_STATUS_PROC;
		
		io_process(proc, head + head->head.head, head->head.head);
		
		head->head.head = head[head->head.head].ce.next;
	}
}

void
user_before_return(proc_t proc)
{
	/* assume the irq is disabled, ensuring no switch */
	
	if (proc->sched_prev_usr != proc)
	{
		if (proc->sched_prev_usr != NULL)
			user_save_context(proc->sched_prev_usr);
		user_restore_context(proc);
	}
	
	/* Now the address base should be of current */

	int busy = *proc->user_thread->iocb.busy;
	
	if (busy == 2 && !user_thread_arch_in_cb_stack())
		busy = 0;
	
	if (busy == 0)
	{
		spinlock_acquire(&proc->user_thread->iocb.push_lock);
		iobuf_index_t head = *proc->user_thread->iocb.head;
		iobuf_index_t tail = *proc->user_thread->iocb.tail;
		spinlock_release(&proc->user_thread->iocb.push_lock);

		if (head != tail)
		{
			busy = 1;
			user_thread_arch_iocb_call();
		}
	}
	
	*proc->user_thread->iocb.busy = busy;
}

int 
user_thread_iocb_push(proc_t proc, iobuf_index_t index)
{
	int irq = irq_save();
	spinlock_acquire(&proc->user_thread->iocb.push_lock);
	
	*proc->user_thread->iocb.tail %= proc->user_thread->iocb.cap;
	proc->user_thread->iocb.entry[*proc->user_thread->iocb.tail] = index;
	(*proc->user_thread->iocb.tail) = ((*proc->user_thread->iocb.tail) + 1) % proc->user_thread->iocb.cap;

	spinlock_release(&proc->user_thread->iocb.push_lock);
	irq_restore(irq);

	return 0;
}

static void
user_set_tls(uintptr_t tls)
{
	/* XXX */
}

void
user_save_context(proc_t proc)
{ user_arch_save_context(proc); }

void
user_restore_context(proc_t proc)
{ user_arch_restore_context(proc); }

/* USER IO PROCESS ============================================ */

static inline int  do_io_page_hole_set(proc_t proc, uintptr_t base, uintptr_t size) __attribute__((always_inline));
static inline int  do_io_page_hole_clear(proc_t proc, uintptr_t base, uintptr_t size) __attribute__((always_inline));
static inline int  do_io_page_hole_guard(proc_t proc, iobuf_index_t idx) __attribute__((always_inline));
static inline int  do_io_phys_alloc(proc_t proc, size_t size, int flags, uintptr_t *result) __attribute__((always_inline));
static inline int  do_io_phys_free(proc_t proc, uintptr_t physaddr) __attribute__((always_inline));
static inline int  do_io_mmio_open(proc_t proc, uintptr_t physaddr, size_t size, uintptr_t *result) __attribute__((always_inline));
static inline int  do_io_mmio_close(proc_t proc, uintptr_t addr) __attribute__((always_inline));
static inline int  do_io_brk(proc_t proc, uintptr_t end) __attribute__((always_inline));
static inline int  do_mbox_open(proc_t proc) __attribute__((always_inline));

static void
io_process(proc_t proc, io_call_entry_t entry, iobuf_index_t idx)
{
	switch (entry->ce.data[0])
	{
	case IO_SET_CALLBACK:
		entry->ce.data[0] = 0;
		proc->user_thread->iocb.callback_u = entry->ce.data[1];
		user_thread_iocb_push(proc, idx);
		break;

	case IO_SET_TLS:
		entry->ce.data[0] = 0;
		user_set_tls(entry->ce.data[1]);
		user_thread_iocb_push(proc, idx);
		break;

	case IO_BRK:
		entry->ce.data[0] = do_io_brk(proc, entry->ce.data[1]);
		user_thread_iocb_push(proc, idx);
		break;

	case IO_EXIT:
		/* XXX */
		user_thread_iocb_push(proc, idx);
		break;

	case IO_PAGE_HOLE_SET:
		entry->ce.data[0] = do_io_page_hole_set(proc, entry->ce.data[1], entry->ce.data[2]);
		user_thread_iocb_push(proc, idx);
		break;

	case IO_PAGE_HOLE_CLEAR:
		entry->ce.data[0] = do_io_page_hole_clear(proc, entry->ce.data[1], entry->ce.data[2]);
		user_thread_iocb_push(proc, idx);
		break;

	case IO_PAGE_HOLE_GUARD:
		entry->ce.data[0] = do_io_page_hole_guard(proc, idx);
		/* iocb would be push when page hole occurs */
		break;

	case IO_DEBUG_PUTCHAR:
		cputchar(entry->ce.data[1]);
		user_thread_iocb_push(proc, idx);
		break;

	case IO_DEBUG_GETCHAR:
		entry->ce.data[1] = cgetchar();
		user_thread_iocb_push(proc, idx);
		break;

	case IO_PHYS_ALLOC:
		entry->ce.data[0] = do_io_phys_alloc(proc, entry->ce.data[1], entry->ce.data[2], &entry->ce.data[1]);
		user_thread_iocb_push(proc, idx);
		break;
		
	case IO_PHYS_FREE:
		entry->ce.data[0] = do_io_phys_free(proc, entry->ce.data[1]);
		user_thread_iocb_push(proc, idx);
		break;
		
	case IO_MMIO_OPEN:
		entry->ce.data[0] = do_io_mmio_open(proc, entry->ce.data[1], entry->ce.data[2], &entry->ce.data[1]);
		user_thread_iocb_push(proc, idx);
		break;

	case IO_MMIO_CLOSE:
		entry->ce.data[0] = do_io_mmio_close(proc, entry->ce.data[1]);
		user_thread_iocb_push(proc, idx);
		break;

	case IO_MBOX_OPEN:
		entry->ce.data[0] = do_mbox_open(proc);
		user_thread_iocb_push(proc, idx);
		break;

	case IO_MBOX_CLOSE:
		/* XXX */
		user_thread_iocb_push(proc, idx);
		break;
		
	case IO_MBOX_IO:
		mbox_io(entry->ce.data[1], entry->ce.data[2], entry->ce.data[3], proc, idx);
		/* iocb would be pushed when request comes */
		break;

	default: break;
	}
}

static inline int
do_io_page_hole_set(proc_t proc, uintptr_t base, uintptr_t size)
{
	if (proc->user_thread->ph_guard == IOBUF_INDEX_NULL)
		return -E_INVAL;

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
do_io_page_hole_guard(proc_t proc, iobuf_index_t idx)
{
	if (proc->user_thread->ph_guard != IOBUF_INDEX_NULL)
		return -E_INVAL;
	
	proc->user_thread->ph_guard = idx;
	return 0;
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
	int r = user_proc_arch_mmio_open(proc->user_proc, physaddr, size, result);
	return r;
}

static inline int
do_io_mmio_close(proc_t proc, uintptr_t addr)
{
	return user_proc_arch_mmio_close(proc->user_proc, addr);
}

static inline int
do_io_brk(proc_t proc, uintptr_t end)
{
	return user_proc_brk(proc->user_proc, end);
}

static inline int
do_mbox_open(proc_t proc)
{
	return mbox_alloc(proc->user_proc);
}
