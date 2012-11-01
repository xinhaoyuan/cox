#include <types.h>
#include <asm/cpu.h>
#include <arch/service.h>
#include "debug.h"

char foostack[4096];

void foo(void)
{
    service_context_s ctx;
    __service_listen(&ctx);

    debug_printf("Recv from %d\n", ctx.args[0]);

    while (1) __cpu_relax();
}

void
__init(void *tls, size_t tls_size, void *start, void *end)
{
    debug_printf("Hello user world.\n");
    
    int tid = __service_sys4(SERVICE_SYS_THREAD_CREATE, (uintptr_t)foo, 0, (uintptr_t)foostack + sizeof(foostack));

    debug_printf("Send to %d\n", tid);

    service_context_s ctx;
    
    ctx.args[0] = tid;
    ctx.args[1] = 999;
    __service_send(&ctx);
    
    while (1) __cpu_relax();
}
