#include <string.h>
#include <bit.h>
#include <irq.h>
#include <page.h>
#include <malloc.h>
#include <spinlock.h>
#include <arch/memlayout.h>
#include <mmu.h>
#include <lib/slab.h>

struct slab_ctrl_s slab_ctrl;
spinlock_s malloc_lock;

void *__slab_page_alloc(size_t num)
{
	return VADDR_DIRECT(PAGE_TO_PHYS(page_alloc_atomic(num)));
}

void __slab_page_free(void *addr)
{
	page_free_atomic(PHYS_TO_PAGE(PADDR_DIRECT(addr)));
}

int
malloc_init(void)
{
	int result;
	slab_ctrl.page_alloc = __slab_page_alloc;
	slab_ctrl.page_free  = __slab_page_free;
	
	if ((result = slab_init(&slab_ctrl)) < 0) return result;
	spinlock_init(&malloc_lock);

	return 0;
}

void *
kmalloc(size_t size)
{
	if (size == 0) return NULL;
	if (size < 8) size = 8;

	void *result;
	
	int irq = irq_save();
	spinlock_acquire(&malloc_lock);

	if (size + SLAB_META_SIZE > SLAB_MAX_ALLOC)
	{
		size_t pagesize = (size + PGSIZE - 1) >> PGSHIFT;
		result = VADDR_DIRECT(PAGE_TO_PHYS(page_alloc_atomic(pagesize)));
	}
	else result = slab_alloc(&slab_ctrl, size);

	spinlock_release(&malloc_lock);
	irq_restore(irq);
	
	return result;
}

void
kfree(void *ptr)
{
	if (ptr == NULL) return;

	int irq = irq_save();
	spinlock_acquire(&malloc_lock);
	
	if (((uintptr_t)ptr & (PGSIZE - 1)) == 0)
	{
		page_free_atomic(PHYS_TO_PAGE(PADDR_DIRECT(ptr)));
	}
	else slab_free(ptr);

	spinlock_release(&malloc_lock);
	irq_restore(irq);
}
