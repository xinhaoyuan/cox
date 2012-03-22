#include <page.h>
#include <spinlock.h>
#include <irq.h>
#include <lib/buddy.h>
#include <mmu.h>
#include <arch/memlayout.h>

static struct buddy_context_s buddy;
static spinlock_s page_atomic_lock;

size_t pages_count;
page_t pages;

void
page_alloc_init_struct(size_t pcount, void*(*init_alloc)(size_t))
{
	buddy_init();
	
	pages_count = pcount;
	buddy.node = init_alloc(BUDDY_CALC_ARRAY_SIZE(pages_count) * sizeof(struct buddy_node_s));
	pages = init_alloc(sizeof(page_s) * pages_count);
	
	spinlock_init(&page_atomic_lock);
}

void
page_alloc_init_layout(int(*layout)(uintptr_t pagenum))
{
	buddy_build(&buddy, pages_count, layout);
}

page_t
page_alloc_atomic(size_t num)
{
	int irq = irq_save();
	spinlock_acquire(&page_atomic_lock);

	buddy_node_id_t result = buddy_alloc(&buddy, num);

	spinlock_release(&page_atomic_lock);
	irq_restore(irq);
	
	if (result == BUDDY_NODE_ID_NULL) return NULL;
	else
	{
		size_t i;
		pages[result].region = num << 1;
		for (i = 1; i < num; ++ i)
			pages[result + i].region = (result << 1) | 1;
		return pages + result;
	}
}

void
page_free_atomic(page_t page)
{
	if (page == 0) return;

	if (page->region & 1)
	{
		page_free_atomic(PAGE_REGION_HEAD(page));
	}
	else
	{
		int irq = irq_save();
		spinlock_acquire(&page_atomic_lock);
		
		buddy_free(&buddy, (page - pages) >> PGSHIFT);
		
		spinlock_release(&page_atomic_lock);
		irq_restore(irq);
	}
}
