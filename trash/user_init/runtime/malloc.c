#include <lib/slab.h>
#include <runtime/page.h>
#include <runtime/malloc.h>

static slab_ctrl_s slab;

int
malloc_init(void)
{
	slab.page_alloc = palloc;
	slab.page_free  = pfree;

	slab_init(&slab);

	return 0;
}

void *
malloc(size_t size)
{
	if (size == 0) return NULL;
	if (size < 8) size = 8;

	void *result;

	if (size + SLAB_META_SIZE > SLAB_MAX_ALLOC)
	{
		size_t pagesize = (size + __PGSIZE - 1) >> __PGSHIFT;
		result = palloc(pagesize);
	}
	else result = slab_alloc(&slab, size);

	return result;
}

void
free(void *ptr)
{
	if (ptr == NULL) return;

	if (((uintptr_t)ptr & (__PGSIZE - 1)) == 0)
	{
		pfree(ptr);
	}
	else slab_free(ptr);
}
