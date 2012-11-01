#include <types.h>
#include <asm/cpu.h>
#include <arch/service.h>

char foostack[4096];

void foo(void)
{
    service_context_s ctx;
        
    ctx.args[0] = 0;
    ctx.args[1] = SERVICE_SYS_LISTEN;
    __service(&ctx);

    ctx.args[0] = 0;
    __service(&ctx);

    while (1) __cpu_relax();
}

void
__init(void *tls, size_t tls_size, void *start, void *end)
{
    service_context_s ctx;
    ctx.args[0] = 0;
    ctx.args[1] = SERVICE_SYS_THREAD_CREATE;
    ctx.args[2] = (uintptr_t)foo;
    ctx.args[3] = 0;
    ctx.args[4] = (uintptr_t)foostack + sizeof(foostack);
    __service(&ctx);

    int tid = ctx.args[1];

    while (1)
    {
        ctx.args[0] = tid;
        ctx.args[1] = 999;
        __service(&ctx);
        if (ctx.args[0] == tid) break;
    }
    
    while (1) __cpu_relax();
}
