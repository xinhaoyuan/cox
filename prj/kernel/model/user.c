#include <malloc.h>
#include <error.h>
#include <proc.h>
#include <user.h>
#include <arch/irq.h>
#include <lib/low_io.h>

static void io_process(proc_t proc, io_call_entry_t entry, iobuf_index_t idx);

int
user_proc_load(void *bin, size_t bin_size)
{
	uintptr_t *ptr = (uintptr_t *)bin;	
	uintptr_t entry = *(ptr ++);
	uintptr_t start = *(ptr ++);
	uintptr_t etext = *(ptr ++);
	uintptr_t edata = *(ptr ++);
	uintptr_t end   = *(ptr ++);

	if (PGOFF(start) || PGOFF(edata) || PGOFF(end)) return -E_INVAL;

	proc_t proc = current;

	if ((proc->usr_mm = kmalloc(sizeof(user_mm_s))) == NULL) return -E_NO_MEM;
	if ((proc->usr_thread = kmalloc(sizeof(user_thread_s))) == NULL) return -E_NO_MEM;

	uintptr_t __start = start - 3 * PGSIZE;
	uintptr_t __end   = end;

	int ret;
	if ((ret = user_thread_init()) != 0) return ret;
	if ((ret = user_mm_init(proc->usr_mm, &__start, &__end)) != 0) return ret;

	/* cb stack */
	if ((ret = user_mm_copy_page(proc->usr_mm, __start, 0, 0))) return ret;
	/* iobuf */
	if ((ret = user_mm_copy_page(proc->usr_mm, __start + PGSIZE, 0, 0))) return ret;
	/* boot stack */
	if ((ret = user_mm_copy_page(proc->usr_mm, __start + 2 * PGSIZE, 0, 0))) return ret;

	uintptr_t now = __start + 3 * PGSIZE;
	while (now < end)
	{
		if (now < edata)
		{
			if ((ret = user_mm_copy_page(proc->usr_mm, now, 0, 0))) return ret;
		}
		else
		{
			/* XXX: on demand */
			if ((ret = user_mm_copy_page(proc->usr_mm, now, 0, 0))) return ret;
		}
		now += PGSIZE;
	}
	
	/* copy the binary */
	user_mm_copy(proc->usr_mm, __start + 3 * PGSIZE, bin, bin_size);

	/* do thread setting */
	user_thread_fill(__start, PGSIZE, (__start + PGSIZE), PGSIZE,
					 (entry + (__end - end)), (uintptr_t)ARCH_STACKTOP(__start + 2 * PGSIZE, PGSIZE));

	return 0;
}

int
user_thread_init(void)
{
	return 0;
}

void
user_thread_fill(uintptr_t cb, size_t cb_size, uintptr_t iobuf, size_t iobuf_size, uintptr_t entry, uintptr_t stacktop)
{
	proc_t proc = current;
	size_t cap = iobuf_size / (sizeof(io_call_entry_s) + sizeof(iobuf_index_t));
	
	proc->usr_thread->iocb.stack = cb;
	proc->usr_thread->iocb.stack_size = cb_size - sizeof(uintptr_t) * 3;
	proc->usr_thread->iocb.busy = (uintptr_t *)(cb + cb_size - sizeof(uintptr_t) * 3);
	proc->usr_thread->iocb.head = (uintptr_t *)(cb + cb_size - sizeof(uintptr_t) * 2);
	proc->usr_thread->iocb.tail = (uintptr_t *)(cb + cb_size - sizeof(uintptr_t));
	proc->usr_thread->iocb.callback = NULL;
	proc->usr_thread->iocb.cap = cap;
	/* set the cb entry from end */
	proc->usr_thread->iocb.entry = (iobuf_index_t *)(iobuf + iobuf_size - sizeof(iobuf_index_t) * cap);
	/* ce entry is from head */
	proc->usr_thread->ioce.cap = cap;
	proc->usr_thread->ioce.head = (io_call_entry_t)iobuf;

	user_thread_arch_fill(entry, stacktop);
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
user_mm_copy_page(user_mm_t mm, uintptr_t addr, uintptr_t phys, int flag)
{
	return user_mm_arch_copy_page(mm, addr, phys, flag);
}

int
user_mm_copy(user_mm_t mm, uintptr_t addr, void *src, size_t size)
{
	return user_mm_arch_copy(mm, addr, src, size);
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
user_before_return(proc_t proc)
{
	if (proc->sched_prev_usr != proc)
	{
		if (proc->sched_prev_usr != NULL)
			user_save_context(proc);
		user_restore_context(proc);
	}
	/* Now the address base should be of current */
	
	/* process IO request */
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
	(*proc->usr_thread->iocb.tail) ++;
}

static void
io_process(proc_t proc, io_call_entry_t entry, iobuf_index_t idx)
{
	switch (entry->ce.data[0])
	{
	case IO_SET_CALLBACK:
		proc->usr_thread->iocb.callback = (io_callback_handler_f)entry->ce.data[1];
		iocb_push(proc, idx);
		break;

	case IO_DEBUG_PUTCHAR:
		cputchar(entry->ce.data[1]);
		iocb_push(proc, idx);
		break;

	default: break;
	}
}

void
user_save_context(proc_t proc)
{ user_arch_save_context(proc); }

void
user_restore_context(proc_t proc)
{ user_arch_restore_context(proc); }
