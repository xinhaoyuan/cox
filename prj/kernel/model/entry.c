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
#include <lwip/sockets.h>
#include <lwip.h>


static proc_s es_proc;
static char   es_stack[10240] __attribute__((aligned(__PGSIZE)));

static void
echo_server(void *__data)
{
    proc_delayed_self_notify_set(200);
    proc_wait_try();
    cprintf("ECHO SERVER START\n");

    int s = lwip_socket(AF_INET, SOCK_STREAM, 0);

    if (s < 0)
    {
        cprintf("SOCKET failed\n");
        return;
    }
    
    struct sockaddr_in sin;
    sin.sin_len = sizeof(sin);
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(7);
    memset(&sin.sin_zero, 0, sizeof(sin.sin_zero));
    
    if (lwip_bind(s, (struct sockaddr *)&sin, sizeof(sin)))
    {
        cprintf("BIND failed\n");
        return;
    }
    
    if (lwip_listen(s, 10))
    {
        cprintf("LISTEN failed\n");
        return;
    }

    cprintf("listening....\n");
    struct sockaddr_in clia;
    u32_t clias;
    while (1)
    {
        clias = sizeof(clia);
        int cli = lwip_accept(s, (struct sockaddr *)&clia, &clias);
        if (cli == -1)
        {
            break;
        }
        cprintf("accepted new client\n");
#define SIZE (1024 * 400)
        char *c = (char *)kmalloc(SIZE);
        int r;
        // cprintf("!! %d to recv\n", timer_tick_get());
        while ((r = lwip_recv(cli, c, SIZE, 0)) > 0)
        {
            // cprintf("!! %d %d\n", timer_tick_get(), r);
            // cprintf("!! %d to send\n", timer_tick_get());
            lwip_send(cli, c, r, 0);
            // cprintf("!! %d to recv\n", timer_tick_get());
        }
        kfree(c);
        lwip_close(cli);
        cprintf("to accept\n");
    }
    
    cprintf("echo server exit\n");
}

void
kernel_start(void)
{
    /* Some kernel-user component initializations here */
    mbox_init();
    mbox_irq_init();
    kern_lwip_init();
    nic_sys_init();

    proc_init(&es_proc, ".echo_server", SCHED_CLASS_RR, (void(*)(void *))echo_server, NULL, (uintptr_t)ARCH_STACKTOP(es_stack, 10240));
    proc_notify(&es_proc);

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
