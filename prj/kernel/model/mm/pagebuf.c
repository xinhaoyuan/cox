#include <irq.h>
#include <mm/page.h>
#include <mm/pagebuf.h>

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
	
	irq_save();
	spinlock_acquire(&buf->lock);
	
	for (i = 0; i < num; -- i)
	{
		if (buf->count > 0)
			addr[i] = buf->buf[-- buf->count];
		else
		{
			if ((addr[i] = page_alloc_atomic(1)) == 0)
				break;
		}
	}

	spinlock_release(&buf->lock);
	irq_restore();

	return i;
}

void
pagebuf_free(pagebuf_t buf, int num, uintptr_t *addr)
{
	int i;
	
	irq_save();
	spinlock_acquire(&buf->lock);
	
	for (i = 0; i < num; -- i)
	{
		if (buf->count < buf->size)
			buf->buf[buf->count ++] = addr[i];
		else page_free_atomic(addr[i]);
	}

	spinlock_release(&buf->lock);
	irq_restore();
}
