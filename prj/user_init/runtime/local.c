#include <runtime/local.h>
#include <runtime/io.h>
#include <runtime/fiber.h>
#include <runtime/sync.h>
#include <runtime/page.h>
#include <runtime/malloc.h>
#include <lib/low_io.h>
#include <user/arch/syscall.h>
#include <user/init.h>
#include <mach.h>

io_data_t  init_io_shadow[16];
upriv_s init_upriv;

#define INIT_THREAD_FIBER_STACK_SIZE 10240
char init_thread_fiber_stack[INIT_THREAD_FIBER_STACK_SIZE] __attribute__((aligned(__PGSIZE)));

void
do_init_thread_fiber(void *entry)
{
    /* alloc the real io shadow */
    __upriv->io_shadow = palloc((__io_cap * sizeof(io_data_t) + __PGSIZE - 1) >> __PGSHIFT);
    ((void(*)(void))entry)();
}


void entry(void);

void
__init(tls_t tls, uintptr_t start, uintptr_t end)
{
    low_io_putc = __debug_putc;
    
    tls_s __tls = *tls;
    __io_cap_set(__tls.info.io_cap);
    __ioce_set(__tls.info.ioce);
    __iocr_set(__tls.info.iocr);
    __iocb_set(__tls.info.iocb);

    if (__thread_arg == 0)
    {
        /* Init thread of a process */
        page_init((void *)(start + (__bin_end - __bin_start)), (void *)end);
        size_t tls_pages = (end - (start + (__bin_end - __bin_start))) >> __PGSHIFT;
        /* XXX: use a dummy palloc to occupy the tls space in heap */
        (void)palloc(tls_pages);
        /* init the slab system */
        (void)malloc_init();
        
        __upriv_set(&init_upriv);
        __upriv->io_shadow = init_io_shadow;

        fiber_sys_init();
        io_init();

        fiber_init(&init_upriv.init_fiber, do_init_thread_fiber, entry, init_thread_fiber_stack, INIT_THREAD_FIBER_STACK_SIZE);
    }
    else
    {
        /* if not the init thread, the fiber sys data should be initialized by creator */
    }
    
    do_idle();
}
