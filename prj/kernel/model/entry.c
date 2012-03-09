#include <cpu.h>
#include <entry.h>
#include <user.h>
#include <page.h>
#include <ips.h>
#include <proc.h>
#include <lib/low_io.h>

proc_s p1, p2;
mutex_s l;

static void
test_1(void)
{
	cprintf("sleep\n");
	proc_delayed_self_notify_set(100);
	proc_wait_try();
	cprintf("wakeup\n");
	/* while (1) */
	/* { */
	/* 	mutex_acquire(&l, NULL); */
	/* 	cprintf("FROM test_1\n"); */
	/* 	mutex_release(&l); */
	/* } */
}

static void
test_2(void)
{
	while (1)
	{
		mutex_acquire(&l, NULL);
		cprintf("FROM test_2\n");
		mutex_release(&l);
	}
}

void
kernel_start(void)
{
	cprintf("KERNEL START\n");
	mutex_init(&l);

	void *stack;

	stack = VADDR_DIRECT(page_alloc_atomic(4));
	proc_init(&p1, ".test-1", SCHED_CLASS_RR, (void(*)(void *))test_1, NULL, (uintptr_t)ARCH_STACKTOP(stack, 4 * PGSIZE));
	proc_notify(&p1);
	
	/* stack = VADDR_DIRECT(page_alloc_atomic(4)); */
	/* proc_init(&p2, ".test-2", SCHED_CLASS_RR, (void(*)(void *))test_2, NULL, (uintptr_t)ARCH_STACKTOP(stack, 4 * PGSIZE)); */
	/* proc_notify(&p2); */
	
	/* extern char _binary_user_init_image_start[]; */
	/* extern char _binary_user_init_image_end[]; */
	/* int ret = user_proc_load((void *)_binary_user_init_image_start, */
	/* 						 _binary_user_init_image_end - _binary_user_init_image_start); */
	/* cprintf("Jump to user\n"); */
	/* user_thread_jump(); */
	
	while (1) __cpu_relax();
}
