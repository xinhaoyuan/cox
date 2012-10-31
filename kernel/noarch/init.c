#define DEBUG_COMPONENT DBG_MISC

#include <init.h>
#include <asm/cpu.h>
#include <debug.h>
#include <irq.h>
#include <proc.h>
#include <timer.h>
#include <user.h>
#include <kmalloc.h>

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
    irq_sys_init_mp();
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

static void
kernel_start(void *__unused)
{
    user_proc_sys_init();
    user_thread_sys_init();

    extern char _user_init_image_start[];
    extern char _user_init_image_end[];
    user_thread_t ut = user_thread_create_from_bin("uinit",
                                          (void *)_user_init_image_start,
                                          _user_init_image_end - _user_init_image_start);

    DEBUG("!!!");
    
    if (ut)
        proc_notify(&ut->proc);
    else PANIC("cannot load user init\n");
}

void
do_idle(void)
{
    while (1) __cpu_relax();
}
