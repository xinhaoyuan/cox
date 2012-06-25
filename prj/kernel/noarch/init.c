#include <init.h>
#include <lib/low_io.h>
#include <asm/mach.h>
#include <irq.h>
#include <proc.h>
#include <kmalloc.h>
#include <user.h>

void
kern_sys_init_global(void)
{
    kmalloc_sys_init();
    irq_sys_init();
    sched_sys_init();
    timer_sys_init();
    timer_master_cpu_set();
}

void
kern_sys_init_local(void)
{
    /* irq buffering */
    irq_sys_init_mp();
    /* put self into idle proc */
    sched_sys_init_mp();
    timer_sys_init_mp();
}

#define INIT_PROC_STACK_SIZE 10240

static char   init_proc_stack[INIT_PROC_STACK_SIZE] __attribute__((aligned(_MACH_PAGE_SIZE)));
static proc_s init_proc;
static void   kernel_start(void *);

void
kern_init(void)
{
    /* create the init proc */
    proc_init(&init_proc, ".init", SCHED_CLASS_RR, (void(*)(void *))kernel_start, NULL,
              init_proc_stack, INIT_PROC_STACK_SIZE);
    proc_notify(&init_proc);
}

static char          user_init_stack[USER_KSTACK_DEFAULT_SIZE] __attribute__((aligned(_MACH_PAGE_SIZE)));
static user_thread_s user_init;

static void
kernel_start(void *__unused)
{
    user_thread_init(&user_init, "uinit", SCHED_CLASS_RR, user_init_stack, USER_KSTACK_DEFAULT_SIZE);
    
    extern char _binary_user_init_image_start[];
    extern char _binary_user_init_image_end[];
    int ret = user_thread_bin_exec(&user_init,
                                   (void *)_binary_user_init_image_start,
                                   _binary_user_init_image_end - _binary_user_init_image_start);
    
    if (ret == 0)
        proc_notify(&user_init.proc);
    else
    {
        PANIC("cannot load user init\n");
    }
}
