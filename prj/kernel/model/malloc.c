#include <string.h>
#include <bit.h>
#include <irq.h>
#include <page.h>
#include <malloc.h>
#include <spinlock.h>
#include <arch/memlayout.h>
#include <mmu.h>

#define EKL_META_SIZE 8
#define EKL_MIN_ALLOC 8
#define EKL_MIN_SHIFT 3
#define EKL_ALLOC_DELTA_SHIFT 10
#define EKL_MAX_SHIFT (EKL_MIN_SHIFT + EKL_ALLOC_DELTA_SHIFT)
#define EKL_MAX_ALLOC (EKL_MIN_ALLOC << EKL_ALLOC_DELTA_SHIFT)
#define EKL_MIN_BLOCK_PAGE 4
#define EKL_MAX_BLOCK_PAGE 256

static char __ekl_size_assert[(EKL_MIN_BLOCK_PAGE << PGSHIFT) >= EKL_MAX_ALLOC ? 0 : -1] __attribute__((unused));

struct ekl_ctrl_s
{
	uint16_t alloc_size;
	uint16_t block_size;
	uintptr_t head;
};


struct mm_ctrl_s
{
	struct ekl_ctrl_s ekl_ctrls[EKL_ALLOC_DELTA_SHIFT + 1];
};

volatile struct mm_ctrl_s mm_ctrl;

static int
ekl_init(void)
{
	int i;
	for (i = 0; i <= EKL_ALLOC_DELTA_SHIFT; ++ i)
	{
		mm_ctrl.ekl_ctrls[i].alloc_size = EKL_MIN_ALLOC << i;
		mm_ctrl.ekl_ctrls[i].head = -1;
		mm_ctrl.ekl_ctrls[i].block_size = EKL_MIN_BLOCK_PAGE;
	}

	return 0;
}

static void *
ekl_ialloc(struct ekl_ctrl_s *ctrl)
{
	if (ctrl->head == (uintptr_t)-1)
	{
		size_t size = ctrl->block_size;
		if (ctrl->block_size < EKL_MAX_BLOCK_PAGE)
			ctrl->block_size <<= 1;
		 
		void *block = VADDR_DIRECT(PAGE_TO_PHYS(page_alloc_atomic(size)));
		size <<= PGSHIFT;
		 
		if (block == NULL)
		{
			return NULL;
		}
		 
		memset(block, 0, size);
		 
		uintptr_t *tail = (uintptr_t *)(
			(uintptr_t)block +
			((size / ctrl->alloc_size - 1) * (ctrl->alloc_size)));
		*tail = -1;
		 
		ctrl->head = (uintptr_t)block;
	}
	 
	uintptr_t *head = (uintptr_t *)ctrl->head;
	ctrl->head = (*head == 0) ? (ctrl->head + ctrl->alloc_size) : (*head);
	 
	*head = (uintptr_t)ctrl;

	return head + 1;
}

static void *
ekl_alloc(size_t size)
{
	int size_index = BIT_SEARCH_LAST(size + EKL_META_SIZE - 1) - EKL_MIN_SHIFT + 1;
	 
	void *result;
	if (size_index < 0 || size_index > EKL_ALLOC_DELTA_SHIFT)
		result = NULL;
	else result = ekl_ialloc((struct ekl_ctrl_s *)mm_ctrl.ekl_ctrls + size_index);

	return result;
}

static void
ekl_free(void *ptr)
{
	if (ptr == NULL) return;
	 
	uintptr_t *head = ((uintptr_t *)ptr - 1);
	struct ekl_ctrl_s *ctrl = (struct ekl_ctrl_s *)*head;

	*head = ctrl->head;
	ctrl->head = (uintptr_t)head;
}

spinlock_s malloc_lock;

int
malloc_init(void)
{
	int result;
	if ((result = ekl_init()) < 0) return result;
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

	if (size + EKL_META_SIZE > EKL_MAX_ALLOC)
	{
		size_t pagesize = (size + PGSIZE - 1) >> PGSHIFT;
		result = VADDR_DIRECT(PAGE_TO_PHYS(page_alloc_atomic(pagesize)));
	}
	else result = ekl_alloc(size);

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
	else ekl_free(ptr);

	spinlock_release(&malloc_lock);
	irq_restore(irq);
}
