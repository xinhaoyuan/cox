#include <cpu.h>
#include <lib/low_io.h>
#include <string.h>
#include <entry.h>
#include <user.h>
#include <page.h>
#include <malloc.h>
#include <ips.h>
#include <proc.h>
#include <mbox.h>
#include <mbox_irq.h>

char user_init_stack[8192] __attribute__((aligned(PGSIZE)));

#define KERN_TEST 1

void
kernel_start(void)
{
    /* Some kernel-user component initializations here */
    mbox_sys_init();
    mbox_irq_init();
    user_thread_sys_init();

#if KERN_TEST

    cprintf("Hello world!\n");

#else
    
    /* Load the user init image */
    user_thread_t user_init = user_thread_get(
        user_thread_alloc(".uinit", SCHED_CLASS_RR, user_init_stack, 8192));
    
    extern char _binary_user_init_image_start[];
    extern char _binary_user_init_image_end[];
    int ret = user_thread_init_exec(&user_init->proc,
                                    (void *)_binary_user_init_image_start,
                                    _binary_user_init_image_end - _binary_user_init_image_start);
    
    if (ret == 0)
        proc_notify(&user_init->proc);
    else
    {
        cprintf("Panic, cannot load user init\n");
    }

#endif

    while (1) __cpu_relax();
}
