#include <cpu.h>
#include <entry.h>
#include <user.h>
#include <lib/low_io.h>

void
kernel_start(void)
{
	cprintf("KERNEL START\n");

	extern char _binary_user_init_image_start[];
	extern char _binary_user_init_image_end[];
	int ret = user_proc_load((void *)_binary_user_init_image_start,
							 _binary_user_init_image_end - _binary_user_init_image_start);
	cprintf("Jump to user\n");
	user_thread_jump();
	
	while (1) __cpu_relax();
}
