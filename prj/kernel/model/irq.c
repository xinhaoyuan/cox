#include <irq.h>
#include <bit.h>
#include <string.h>
#include <error.h>

#include <mm/malloc.h>
#include <lib/low_io.h>

#include <arch/local.h>
#include <arch/intr.h>

PLS_ATOM_DEFINE(int, __irq_save, 0);

typedef struct irq_info_s
{
	uint64_t irq_mask;
	uint64_t irq_accumulate[IRQ_COUNT];
} irq_info_s;

PLS_PTR_DEFINE(irq_info_s, __irq_info, NULL);
irq_handler_f irq_handler[IRQ_COUNT];

int
irq_init(void)
{
	memset(irq_handler, 0, sizeof(irq_handler));
	return 0;
}

int
irq_init_mp(void)
{
	irq_info_s *info = kmalloc(sizeof(irq_info_s));
	if (info == NULL) return -E_NO_MEM;
	
	PLS_SET(__irq_save, 0);
	info->irq_mask = 0;
	memset(info->irq_accumulate, 0, sizeof(info->irq_accumulate));
	PLS_SET(__irq_info, info);
	
	return 0;
}

static void
irq_process(void)
{
	int irq_no;
	irq_info_s *info = PLS(__irq_info);
	irq_handler_f h;
	uint64_t acc;
	/* Assume irq is hw-disabled  */
	while (info->irq_mask)
	{
		irq_no = BIT_SEARCH_LAST(info->irq_mask);
		h      = irq_handler[irq_no];
		if (h)
		{
			acc = info->irq_accumulate[irq_no];
			
			info->irq_accumulate[irq_no] = 0;
			info->irq_mask ^= 1 << irq_no;

			intr_enable();
			h(irq_no, acc);
			intr_disable();
		}
		else
		{
			info->irq_mask ^= 1 << irq_no;
		}
	}
}

int
irq_save(void)
{
	int intr = intr_save();
	int ret = PLS(__irq_save);
	PLS_SET(__irq_save, 1);
	intr_restore(intr);
	return ret;
}

void
irq_restore(int state)
{
	int intr = intr_save();	
	struct irq_info_s *info = PLS(__irq_info);
	PLS_SET(__irq_save, state);
	if (info && state == 0 && info->irq_mask)
	{
		irq_process();
	}
	intr_restore(intr);
}

void
irq_entry(int irq)
{
	int intr = intr_save();
	irq_info_s *info = PLS(__irq_info);

	info->irq_mask |= (1 << irq);
	++ info->irq_accumulate[irq];
	
	if (PLS(__irq_save) == 0)
	{
		PLS_SET(__irq_save, 1);
		irq_process();
		PLS_SET(__irq_save, 0);
	}

	intr_restore(intr);
}
