#include <timer.h>
#include <irq.h>
#include <proc.h>
#include <lib/low_io.h>
#include <arch/intr.h>

static void timer_tick(int irq_no, uint64_t acc);

int
timer_init(void)
{
	irq_handler[IRQ_TIMER] = timer_tick;
	return 0;
}

static void
timer_tick(int irq_no, uint64_t acc)
{
	schedule();
}
