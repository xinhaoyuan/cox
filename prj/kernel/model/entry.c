#include <cpu.h>
#include <string.h>
#include <entry.h>
#include <user.h>
#include <page.h>
#include <malloc.h>
#include <ips.h>
#include <proc.h>
#include <lib/low_io.h>
#include <lwip.h>
#include <mbox.h>

static proc_s p;
static char   p_stack[10240];

void
test(void *__ignore)
{
	proc_delayed_self_notify_set(200);
	proc_wait_try();
	cprintf("TEST NIC WRITE\n");

#define STR1 "Hello world"
#define STR2 "Hello world again"
		
	static char buf1[] = STR1;
	static int  len1 = sizeof(STR1);
	static char buf2[] = STR2;
	static int  len2 = sizeof(STR2);

	mbox_send(0, buf1, len1, 0, len1);
	while (1)
	{
		cprintf("Send more packet!\n");
		mbox_send(0, buf2, len2, 0, len2);
	}
	
	while (1) __cpu_relax();
}

void
kernel_start(void)
{
	mbox_init();

	proc_init(&p, ".test", SCHED_CLASS_RR, (void(*)(void *))test, NULL, (uintptr_t)ARCH_STACKTOP(p_stack, 10240));
	proc_notify(&p);
	
	/* Load the user init image */
	
	extern char _binary_user_init_image_start[];
	extern char _binary_user_init_image_end[];
	int ret = user_proc_load((void *)_binary_user_init_image_start,
							 _binary_user_init_image_end - _binary_user_init_image_start);
	user_thread_jump();

	while (1) __cpu_relax();
}
