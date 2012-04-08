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
#include <nic.h>
#include <mbox_irq.h>

void
kernel_start(void)
{
    /* Some kernel-user component initializations here */
    mbox_init();
    mbox_irq_init();
    kern_lwip_init();
    nic_sys_init();

    /* Load the user init image */
    
    extern char _binary_user_init_image_start[];
    extern char _binary_user_init_image_end[];
    int ret = user_proc_load((void *)_binary_user_init_image_start,
                             _binary_user_init_image_end - _binary_user_init_image_start);

    if (ret == 0)
        user_thread_jump();
    else
    {
        cprintf("Panic, cannot load user init\n");
    }

    while (1) __cpu_relax();
}
