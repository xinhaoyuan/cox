#include <init.h>
#include <lib/low_io.h>
#include <asm/mach.h>
#include <irq.h>
#include <proc.h>
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

static void
kernel_start(void *__unused)
{
    cprintf("Hello world!\n");
}
