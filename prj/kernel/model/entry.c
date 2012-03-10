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

void
kernel_start(void)
{
	extern char _binary_user_init_image_start[];
	extern char _binary_user_init_image_end[];
	int ret = user_proc_load((void *)_binary_user_init_image_start,
							 _binary_user_init_image_end - _binary_user_init_image_start);
	user_thread_jump();

	while (1) __cpu_relax();
}
