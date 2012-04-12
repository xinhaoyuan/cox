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

char                  user_init_stack[8192] __attribute__((aligned(PGSIZE)));
user_thread_wrapper_s user_init;

void
kernel_start(void)
{
    /* Some kernel-user component initializations here */
    mbox_sys_init();
    mbox_irq_init();

    /* Load the user init image */
    memset(&user_init.proc, 0, sizeof(user_init));
    proc_init(&user_init.proc, ".uinit", SCHED_CLASS_RR, (void(*)(void *))user_thread_jump, NULL, user_init_stack, 8192);

    extern char _binary_user_init_image_start[];
    extern char _binary_user_init_image_end[];
    int ret = user_thread_init_exec(&user_init.proc,
                                    (void *)_binary_user_init_image_start,
                                    _binary_user_init_image_end - _binary_user_init_image_start);
    if (ret == 0)
        proc_notify(&user_init.proc);
    else
    {
        cprintf("Panic, cannot load user init\n");
    }

    while (1) __cpu_relax();
}
