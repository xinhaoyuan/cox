#include <lib/slab.h>
#include <bit.h>
#include <string.h>

int
slab_init(slab_ctrl_t ctrl)
{
	int i;
	for (i = 0; i <= SLAB_ALLOC_DELTA_SHIFT; ++ i)
	{
		ctrl->class_ctrls[i].alloc_size = SLAB_MIN_ALLOC << i;
		ctrl->class_ctrls[i].head = -1;
		ctrl->class_ctrls[i].block_size = SLAB_MIN_BLOCK_PAGE;
	}

	return 0;
}

static void *
slab_class_alloc(slab_ctrl_t slab, slab_class_ctrl_t class)
{
	if (class->head == (uintptr_t)-1)
	{
		size_t size = class->block_size;
		if (class->block_size < SLAB_MAX_BLOCK_PAGE)
			class->block_size <<= 1;
		 
		void *block = slab->page_alloc(size);
		size <<= __PGSHIFT;
		 
		if (block == NULL)
		{
			return NULL;
		}
		 
		memset(block, 0, size);
		 
		uintptr_t *tail = (uintptr_t *)(
			(uintptr_t)block +
			((size / class->alloc_size - 1) * (class->alloc_size)));
		*tail = -1;
		 
		class->head = (uintptr_t)block;
	}
	 
	uintptr_t *head = (uintptr_t *)class->head;
	class->head = (*head == 0) ? (class->head + class->alloc_size) : (*head);
	 
	*head = (uintptr_t)class;

	return head + 1;
}

void *
slab_alloc(slab_ctrl_t slab, size_t size)
{
	int size_index = BIT_SEARCH_LAST(size + SLAB_META_SIZE - 1) - SLAB_MIN_SHIFT + 1;
	 
	void *result;
	if (size_index < 0 || size_index > SLAB_ALLOC_DELTA_SHIFT)
		result = NULL;
	else result = slab_class_alloc(slab, slab->class_ctrls + size_index);

	return result;
}

void
slab_free(void *ptr)
{
	if (ptr == NULL) return;
	 
	uintptr_t *head = ((uintptr_t *)ptr - 1);
	slab_class_ctrl_t ctrl = (slab_class_ctrl_t )*head;

	*head = ctrl->head;
	ctrl->head = (uintptr_t)head;
}
