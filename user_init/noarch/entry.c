#include <asm/cpu.h>
#include <arch/service.h>

void
__init(void *tls, size_t tls_size, void *start, void *end)
{
    service_context_s ctx;
    ctx.args[0] = 0;
    ctx.args[1] = SERVICE_SYS_LISTEN;
    __service(&ctx);
    
    while (1) __cpu_relax();
}
