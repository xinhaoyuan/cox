#include <asm/cpu.h>
#include <arch/syscall.h>

void
__init(void *tls, size_t tls_size, void *start, void *end)
{
    __debug_putc(':');
    __debug_putc(')');
    while (1) __cpu_relax();
}
