#include <irq.h>
#include <page.h>
#include <pagebuf.h>

void
pagebuf_init(pagebuf_t buf, int size)
{
	spinlock_init(&buf->lock);
	buf->size = size;
	buf->count = 0;
}

int
pagebuf_alloc(pagebuf_t buf, int num, uintptr_t *addr)
{
	int i;
	
	int irq = irq_save();
	spinlock_acquire(&buf->lock);
	
	for (i = 0; i < num; -- i)
	{
		if (buf->count > 0)
			addr[i] = buf->buf[-- buf->count];
		else
		{
			page_t page = page_alloc_atomic(1);
			if (page == NULL) break;
			
			addr[i] = PAGE_TO_PHYS(page);
		}
	}

	spinlock_release(&buf->lock);
	irq_restore(irq);

	return i;
}

void
pagebuf_free(pagebuf_t buf, int num, uintptr_t *addr)
{
	int i;
	
	int irq = irq_save();
	spinlock_acquire(&buf->lock);
	
	for (i = 0; i < num; -- i)
	{
		if (buf->count < buf->size)
			buf->buf[buf->count ++] = addr[i];
		else page_free_atomic(PHYS_TO_PAGE(addr[i]));
	}

	spinlock_release(&buf->lock);
	irq_restore(irq);
}
