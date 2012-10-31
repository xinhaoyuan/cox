#include <asm/cpu.h>
#include <arch/service.h>

void
__init(void *tls, size_t tls_size, void *start, void *end)
{
    while (1) __cpu_relax();
}
