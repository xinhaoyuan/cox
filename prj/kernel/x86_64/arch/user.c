#include <cpu.h>
#include <string.h>
#include <kernel/error.h>
#include <kernel/proc.h>
#include <kernel/user.h>
#include <mm/page.h>
#include <arch/user.h>
#include <arch/intr.h>
#include <arch/mem.h>
#include <user/info.h>

int
user_thread_arch_push_iocb(iobuf_index_t head, iobuf_index_t tail)
{
	proc_t proc = current;
	if (proc->usr_thread == NULL) return -E_INVAL;
	if (proc->usr_thread->iocb.callback == NULL) return -E_INVAL;
	if (proc->usr_thread->arch.tf == NULL) return -E_INVAL;

#if 0 							/* ensured by whole os */
	/* assume no page fault */
	if (PGOFF(proc->usr_thread->arch.stacktop) < sizeof(struct trapframe) + 128) return -E_INVAL;
	pte_t *pte = get_pte(proc->usr.arch->mm->pgdir, proc->usr.arch.stacktop, 0);
	if (pte == NULL ||
		(*pte & (PTE_P | PTE_W | PTE_U)) != (PTE_P | PTE_W | PTE_U)) return -E_INVAL;
#endif


	if (*proc->usr_thread->iocb.busy != 0) return -E_BUSY;
	char *stacktop = (char *)proc->usr_thread->iocb.stacktop;
	
	stacktop -= sizeof(struct trapframe);
	memmove(stacktop, proc->usr_thread->arch.tf, sizeof(struct trapframe));

	proc->usr_thread->arch.tf->tf_rip = (uintptr_t)proc->usr_thread->iocb.callback;
	proc->usr_thread->arch.tf->tf_rsp = (uintptr_t)stacktop;
	/* calling conversion */	
	proc->usr_thread->arch.tf->tf_regs.reg_rdi = head;
	proc->usr_thread->arch.tf->tf_regs.reg_rsi = tail;

	return 0;
}

int
user_thread_arch_init(void)
{
	proc_t proc = current;
	memset(&proc->usr_thread->arch, 0, sizeof(proc->usr_thread->arch));
	return 0;
}

void user_thread_arch_fill(uintptr_t entry, uintptr_t stacktop)
{
	proc_t proc = current;
	if (proc->usr_thread->arch.tf != NULL)
	{
		proc->usr_thread->arch.tf->tf_rip = entry;
		proc->usr_thread->arch.tf->tf_rsp = stacktop;
	}
	else
	{
		proc->usr_thread->arch.init_entry = entry;
		stacktop -= sizeof(startup_info_s);
		startup_info_s info;
		info.ioce.cap   = proc->usr_thread->ioce.cap;
		info.ioce.head  = proc->usr_thread->ioce.head;
		info.iocb.cap   = proc->usr_thread->iocb.cap;
		info.iocb.entry = proc->usr_thread->iocb.entry;
		user_mm_arch_copy(proc->usr_mm, stacktop, &info, sizeof(info));
		proc->usr_thread->arch.init_stacktop = stacktop;
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
		tf.tf_rip = proc->usr_thread->arch.init_entry;
		tf.tf_rsp = proc->usr_thread->arch.init_stacktop;
		__user_jump(&tf);
	}
}

int
user_mm_arch_init(user_mm_t mm, uintptr_t *start, uintptr_t *end)
{
	uintptr_t pgdir_phys = page_alloc_atomic(1);
	if (pgdir_phys == 0) return -E_NO_MEM;

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
		uintptr_t phys = page_alloc_atomic(1);
		*pte = phys | PTE_W | PTE_U | PTE_P;
	}
	memmove(VADDR_DIRECT(PTE_ADDR(*pte)), VADDR_DIRECT(phys), PGSIZE);
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

void
user_arch_before_return(proc_t proc)
{
	if (proc->switched)
	{
		__lcr3(proc->usr_mm->arch.cr3);
	}
}
