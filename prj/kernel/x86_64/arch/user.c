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
	t->iocb.busy = t->tls;
	t->iocb.head = t->tls + sizeof(uintptr_t);
	t->iocb.tail = t->tls + sizeof(uintptr_t) * 2;
	t->ioce.head = t->tls + PGSIZE * 2;
	t->iocb.entry = t->tls + PGSIZE * 3 - t->iocb.cap * sizeof(iobuf_index_t);

	void *cur = t->tls + t->user_size; 
	t->iocb.stack = cur; cur += t->iocb_stack_size;
	t->iocb.entry = (iobuf_index_t *)(cur + t->iobuf_size - sizeof(iobuf_index_t) * t->iocb.cap);
	t->ioce.head = (io_call_entry_t)cur;
	
	__lcr3(__rcr3());
	
	tls_s tls;
	tls.iocb_busy = tls.iocb_head = tls.iocb_tail = 0;
	tls.startup_info.ioce.cap   = t->ioce.cap;
	tls.startup_info.ioce.head  = (void *)(t->tls_u + ((char *)t->ioce.head - (char *)t->tls));
	tls.startup_info.iocb.cap   = t->iocb.cap;
	tls.startup_info.iocb.entry = (void *)(t->tls_u + ((char *)t->iocb.entry - (char *)t->tls));
	
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
		tf.tf_rflags = FL_IF;
		tf.tf_rip = proc->usr_thread->arch.init_entry;
		tf.tf_regs.reg_rdi = proc->usr_thread->tls_u;
		__user_jump(&tf);
	}
}

int
user_mm_arch_init(user_mm_t mm, uintptr_t *start, uintptr_t *end)
{
	page_t pgdir_page = page_alloc_atomic(1);
	if (pgdir_page == NULL) return -E_NO_MEM;
	uintptr_t pgdir_phys = PAGE_TO_PHYS(pgdir_page);

	mm->arch.cr3   = pgdir_phys;
	mm->arch.pgdir = VADDR_DIRECT(pgdir_phys);
	/* copy from the scratch */
	memset(mm->arch.pgdir, 0, PGSIZE);
	memmove(mm->arch.pgdir + PGX(PHYSBASE), pgdir_scratch + PGX(PHYSBASE), (NPGENTRY - PGX(PHYSBASE)) * sizeof(pgd_t));
	mm->arch.pgdir[PGX(VPT)] = pgdir_phys | PTE_W | PTE_P;
	
	return 0;
}

int
user_mm_arch_copy_page(user_mm_t mm, uintptr_t addr, uintptr_t phys, int flag)
{
	pte_t *pte = get_pte(mm->arch.pgdir, addr, 1);
	if (!(*pte & PTE_P))
	{
		uintptr_t phys = PAGE_TO_PHYS(page_alloc_atomic(1));
		*pte = phys | PTE_W | PTE_U | PTE_P;
	}
	if (phys == 0)
	{
		memset(VADDR_DIRECT(PTE_ADDR(*pte)), 0, PGSIZE);
	}
	else
	{
		memmove(VADDR_DIRECT(PTE_ADDR(*pte)), VADDR_DIRECT(phys), PGSIZE);
	}
	return 0;
}

int
user_mm_arch_copy(user_mm_t mm, uintptr_t addr, void *src, size_t size)
{
	uintptr_t ptr = addr;
	uintptr_t end = ptr + size;
	char *__src = (char *)src - PGOFF(ptr);
	ptr -= PGOFF(ptr);

	while (ptr < end)
	{
		pte_t *pte = get_pte(mm->arch.pgdir, ptr, 0);
		if (pte == NULL) return -E_INVAL;
		if ((*pte & (PTE_P | PTE_W | PTE_U)) != (PTE_P | PTE_W | PTE_U)) return -E_INVAL;

		if (ptr < addr)
		{
			if (ptr + PGSIZE >= end)
			{
				memmove(VADDR_DIRECT(PTE_ADDR(*pte) + PGOFF(addr)),
						__src + PGOFF(addr), size);
			}
			else
			{
				memmove(VADDR_DIRECT(PTE_ADDR(*pte) + PGOFF(addr)),
						__src + PGOFF(addr), PGSIZE - PGOFF(addr));
			}
		}
		else
		{
			if (ptr + PGSIZE >= end)
			{
				memmove(VADDR_DIRECT(PTE_ADDR(*pte)),
						__src, end - ptr);
			}
			else
			{
				memmove(VADDR_DIRECT(PTE_ADDR(*pte)),
						__src, PGSIZE);
			}
		}

		ptr   += PGSIZE;
		__src += PGSIZE;
	}
	
	return 0;
}

void *
user_mm_arch_memmap_open(user_mm_t mm, uintptr_t addr, size_t size)
{
	return NULL;
}

void
user_mm_arch_memmap_close(user_mm_t mm, void *addr)
{
}

void
user_arch_save_context(proc_t proc)
{ }

void
user_arch_restore_context(proc_t proc)
{
	__lcr3(proc->usr_mm->arch.cr3);
}
