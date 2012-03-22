#include <cpu.h>
#include <string.h>
#include <error.h>
#include <proc.h>
#include <user.h>
#include <page.h>
#include <arch/user.h>
#include <user/tls.h>

#include "intr.h"
#include "mem.h"

int
user_thread_arch_push_iocb(void)
{
	proc_t proc = current;
	if (proc->usr_thread == NULL) return -E_INVAL;
	if (proc->usr_thread->iocb.callback_u == 0) return -E_INVAL;
	if (proc->usr_thread->arch.tf == NULL) return -E_INVAL;

#if 0 							/* ensured by whole os */
	/* assume no page fault */
	if (PGOFF(proc->usr_thread->arch.stacktop) < sizeof(struct trapframe) + 128) return -E_INVAL;
	pte_t *pte = get_pte(proc->usr.arch->mm->pgdir, proc->usr.arch.stacktop, 0);
	if (pte == NULL ||
		(*pte & (PTE_P | PTE_W | PTE_U)) != (PTE_P | PTE_W | PTE_U)) return -E_INVAL;
#endif

	char *stacktop = ARCH_STACKTOP(proc->usr_thread->iocb.stack, proc->usr_thread->iocb_stack_size);
	
	stacktop -= sizeof(struct trapframe);
	memmove(stacktop, proc->usr_thread->arch.tf, sizeof(struct trapframe));

	proc->usr_thread->arch.tf->tf_regs.reg_rbp = 0;
	proc->usr_thread->arch.tf->tf_rip = proc->usr_thread->iocb.callback_u;
	proc->usr_thread->arch.tf->tf_rsp = (uintptr_t)stacktop;
	/* pass the trapframe addr */
	proc->usr_thread->arch.tf->tf_regs.reg_rdi = (uintptr_t)stacktop;

	return 0;
}

int
user_thread_arch_init(proc_t proc, uintptr_t entry)
{
	memset(&proc->usr_thread->arch, 0, sizeof(proc->usr_thread->arch));
	proc->usr_thread->arch.init_entry = entry;
	user_thread_t t = proc->usr_thread;
	if (t->user_size + t->iocb_stack_size + t->iobuf_size != PGSIZE * 3)
	{
		cprintf("PANIC: initial tls size != PGSIZE * 3");
		return -E_INVAL;
	}
	/* XXX to cleanup */
	t->tls = valloc(3);
	*get_pte(pgdir_scratch, (uintptr_t)t->tls, 1) = *get_pte(proc->usr_mm->arch.pgdir, t->tls_u, 0);
	*get_pte(pgdir_scratch, (uintptr_t)t->tls + PGSIZE, 1) = *get_pte(proc->usr_mm->arch.pgdir, t->tls_u + PGSIZE, 0);
	*get_pte(pgdir_scratch, (uintptr_t)t->tls + PGSIZE * 2, 1) = *get_pte(proc->usr_mm->arch.pgdir, t->tls_u + PGSIZE * 2, 0);

	/* refer to user/tls.h */
	t->iocb.busy  = t->tls + OFFSET_OF(tls_s, iocb.busy);
	t->iocb.head  = t->tls + OFFSET_OF(tls_s, iocb.head);
	t->iocb.tail  = t->tls + OFFSET_OF(tls_s, iocb.tail);
	t->ioce.head  = t->tls + PGSIZE * 2;
	t->iocb.entry = t->tls + PGSIZE * 3 - t->iocb.cap * sizeof(iobuf_index_t);

	void *cur = t->tls + t->user_size; 
	t->iocb.stack = cur; cur += t->iocb_stack_size;
	t->iocb.entry = (iobuf_index_t *)(cur + t->iobuf_size - sizeof(iobuf_index_t) * t->iocb.cap);
	t->ioce.head = (io_call_entry_t)cur;
	
	__lcr3(__rcr3());
	
	tls_s tls;
	/* XXX */
	tls.proc_arg   = 0;
	tls.thread_arg = 0;
	tls.iocb.busy = tls.iocb.head = tls.iocb.tail = 0;
	tls.info.ioce.cap   = t->ioce.cap;
	tls.info.ioce.head  = (void *)(t->tls_u + ((char *)t->ioce.head - (char *)t->tls));
	tls.info.iocb.cap   = t->iocb.cap;
	tls.info.iocb.entry = (void *)(t->tls_u + ((char *)t->iocb.entry - (char *)t->tls));
	
	user_mm_arch_copy(proc->usr_mm, t->tls_u, &tls, sizeof(tls));
	return 0;
}

int
user_thread_arch_in_cb_stack(void)
{
	proc_t proc = current;
	if (proc->usr_thread->arch.tf != NULL)
	{
		return 0;
	}
	else
	{
		uintptr_t stack = proc->usr_thread->arch.tf->tf_rsp;
		return proc->usr_thread->iocb.stack_u <= stack &&
			stack < proc->usr_thread->iocb.stack_u + proc->usr_thread->iocb_stack_size;
	}
}

void __user_jump(struct trapframe *tf) __attribute__((noreturn));

void
user_thread_arch_jump(void)
{
	proc_t proc = current;
	if (proc->usr_thread->arch.tf != NULL)
	{
		__user_jump(proc->usr_thread->arch.tf);
	}
	else
	{
		struct trapframe tf;
		memset(&tf, 0, sizeof(tf));
		tf.tf_cs = GD_UTEXT | 3;
		tf.tf_ss = GD_UDATA | 3;
		tf.tf_ds = GD_UDATA | 3;
		tf.tf_es = GD_UDATA | 3;
		tf.tf_rflags = FL_IF ;
		/* XXX: for test driver node */
		tf.tf_rflags |= FL_IOPL_3;
		tf.tf_rip = proc->usr_thread->arch.init_entry;
		tf.tf_regs.reg_rdi = proc->usr_thread->tls_u;
		__user_jump(&tf);
	}
}
