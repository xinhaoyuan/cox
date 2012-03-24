#include <malloc.h>
#include <error.h>
#include <proc.h>
#include <user.h>
#include <page.h>
#include <arch/irq.h>
#include <lib/low_io.h>

static void io_process(proc_t proc, io_call_entry_t entry, iobuf_index_t idx);
static int user_mm_init(user_mm_t mm, uintptr_t *start, uintptr_t *end);
static int user_thread_init(proc_t proc, uintptr_t entry);
static void print_tls(proc_t proc)
{		
	cprintf("tls        = 0x%016lx tls_u        = 0x%016lx\n",
			proc->usr_thread->tls, proc->usr_thread->tls_u);
	cprintf("iocb_busy  = 0x%016lx iocb_cap     = %d\n"
			"iocb_head  = 0x%016lx iocb_tail    = 0x%016lx\n",
			proc->usr_thread->iocb.busy, proc->usr_thread->iocb.cap,
			proc->usr_thread->iocb.head, proc->usr_thread->iocb.tail);
	cprintf("iocb_entry = 0x%016lx ioce_head    = 0x%016lx\n",
			proc->usr_thread->iocb.entry, proc->usr_thread->ioce.head);
	cprintf("iocb_stack = 0x%016lx iocb_stack_u = 0x%016lx\n",
			proc->usr_thread->iocb.stack, proc->usr_thread->iocb.stack_u);
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

	if ((proc->usr_mm = kmalloc(sizeof(user_mm_s))) == NULL) return -E_NO_MEM;
	if ((proc->usr_thread = kmalloc(sizeof(user_thread_s))) == NULL) return -E_NO_MEM;

	uintptr_t __start = start;
	/* 3 pages for initial tls */
	uintptr_t __end   = end + 3 * PGSIZE;

	int ret;
	if ((ret = user_mm_init(proc->usr_mm, &__start, &__end)) != 0) return ret;
	if ((ret = user_thread_init(proc, entry + __start - start)) != 0) return ret;

	uintptr_t now = start;
	while (now < end)
	{
		if (now < edata)
		{
			if ((ret = user_mm_copy_page(proc->usr_mm, now + __start - start, 0, 0))) return ret;
		}
		else
		{
			/* XXX: on demand */
			if ((ret = user_mm_copy_page(proc->usr_mm, now + __start - start, 0, 0))) return ret;
		}
		now += PGSIZE;
	}
	
	/* copy the binary */
	user_mm_copy(proc->usr_mm, __start, bin, bin_size);
	print_tls(proc);
	return 0;
}

int
user_mm_init(user_mm_t mm, uintptr_t *start, uintptr_t *end)
{
	int ret;
	if ((ret = user_mm_arch_init(mm, start, end))) return ret;

	mm->start = *start;
	mm->end =   *end;

	return 0;
}

int
user_thread_init(proc_t proc, uintptr_t entry)
{
	user_thread_t t = proc->usr_thread;
	t->user_size = PGSIZE;
	t->iobuf_size = PGSIZE;
	t->iocb_stack_size = PGSIZE;
	t->tls_u = proc->usr_mm->end - 3 * PGSIZE;

	/* touch the memory */
	user_mm_arch_copy_page(proc->usr_mm, t->tls_u, 0, 0);
   	user_mm_arch_copy_page(proc->usr_mm, t->tls_u + PGSIZE, 0, 0);
	user_mm_arch_copy_page(proc->usr_mm, t->tls_u + PGSIZE * 2, 0, 0);
	
	t->iocb.stack_u = t->tls_u + t->user_size;
	proc->usr_thread->iocb.callback_u = 0;

	size_t cap = t->iobuf_size / (sizeof(io_call_entry_s) + sizeof(iobuf_index_t));
	
	proc->usr_thread->iocb.cap = cap;
	proc->usr_thread->ioce.cap = cap;

	return user_thread_arch_init(proc, entry);
}

int
user_mm_copy_page(user_mm_t mm, uintptr_t addr, uintptr_t phys, int flag)
{
	return user_mm_arch_copy_page(mm, addr, phys, flag);
}

int
user_mm_copy(user_mm_t mm, uintptr_t addr, void *src, size_t size)
{
	return user_mm_arch_copy(mm, addr, src, size);
}

int
user_mm_brk(user_mm_t mm, uintptr_t end)
{
	cprintf("BRK: %016lx\n", end);
	
	if (end <= mm->start) return -E_INVAL;
	if (end & (PGSIZE - 1)) return -E_INVAL;
	int ret = user_mm_arch_brk(mm, end);
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
	io_call_entry_t head = proc->usr_thread->ioce.head;
	while (head->head.head != 0 &&
		   head->head.head != head->head.tail)
	{
		head->head.head %= proc->usr_thread->ioce.cap;

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
			user_save_context(proc);
		user_restore_context(proc);
	}
	
	/* Now the address base should be of current */

	int busy = *proc->usr_thread->iocb.busy;
	
	if (busy == 2 && !user_thread_arch_in_cb_stack())
		busy = 0;
	
	if (busy == 0)
	{
		iobuf_index_t head = *proc->usr_thread->iocb.head;
		iobuf_index_t tail = *proc->usr_thread->iocb.tail;

		if (head != tail)
		{
			busy = 1;
			user_thread_arch_push_iocb();
		}
	}
	
	*proc->usr_thread->iocb.busy = busy;
}

static void
iocb_push(proc_t proc, iobuf_index_t idx)
{
	*proc->usr_thread->iocb.tail %= proc->usr_thread->iocb.cap;
	proc->usr_thread->iocb.entry[*proc->usr_thread->iocb.tail] = idx;
	(*proc->usr_thread->iocb.tail) = ((*proc->usr_thread->iocb.tail) + 1) % proc->usr_thread->iocb.cap;
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

static inline int do_io_phys_alloc(proc_t proc, size_t size, int flags, uintptr_t *result) __attribute__((always_inline));
static inline int do_io_phys_free(proc_t proc, uintptr_t physaddr) __attribute__((always_inline));
static inline int do_io_mmio_open(proc_t proc, uintptr_t physaddr, size_t size, uintptr_t *result) __attribute__((always_inline));
static inline int do_io_mmio_close(proc_t proc, uintptr_t addr) __attribute__((always_inline));
static inline int do_io_brk(proc_t proc, uintptr_t end) __attribute__((always_inline));

static void
io_process(proc_t proc, io_call_entry_t entry, iobuf_index_t idx)
{
	switch (entry->ce.data[0])
	{
	case IO_SET_CALLBACK:
		entry->ce.data[0] = 0;
		proc->usr_thread->iocb.callback_u = entry->ce.data[1];
		iocb_push(proc, idx);
		break;

	case IO_SET_TLS:
		entry->ce.data[0] = 0;
		user_set_tls(entry->ce.data[1]);
		iocb_push(proc, idx);
		break;

	case IO_BRK:
		entry->ce.data[0] = do_io_brk(proc, entry->ce.data[1]);
		iocb_push(proc, idx);
		break;

	case IO_EXIT:
		/* XXX */
		iocb_push(proc, idx);
		break;

	case IO_DEBUG_PUTCHAR:
		cputchar(entry->ce.data[1]);
		iocb_push(proc, idx);
		break;

	case IO_DEBUG_GETCHAR:
		entry->ce.data[1] = cgetchar();
		iocb_push(proc, idx);
		break;

	case IO_PHYS_ALLOC:
		entry->ce.data[0] = do_io_phys_alloc(proc, entry->ce.data[1], entry->ce.data[2], &entry->ce.data[1]);
		iocb_push(proc, idx);
		break;
		
	case IO_PHYS_FREE:
		entry->ce.data[0] = do_io_phys_free(proc, entry->ce.data[1]);
		iocb_push(proc, idx);
		break;
		
	case IO_MMIO_OPEN:
		entry->ce.data[0] = do_io_mmio_open(proc, entry->ce.data[1], entry->ce.data[2], &entry->ce.data[1]);
		iocb_push(proc, idx);
		break;

	case IO_MMIO_CLOSE:
		entry->ce.data[0] = do_io_mmio_close(proc, entry->ce.data[1]);
		iocb_push(proc, idx);
		break;

	default: break;
	}
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
	return user_mm_arch_mmio_open(proc->usr_mm, physaddr, size, result);
}

static inline int
do_io_mmio_close(proc_t proc, uintptr_t addr)
{
	return user_mm_arch_mmio_close(proc->usr_mm, addr);
}

static inline int
do_io_brk(proc_t proc, uintptr_t end)
{
	return user_mm_brk(proc->usr_mm, end);
}
