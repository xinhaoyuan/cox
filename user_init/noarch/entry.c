#include <types.h>
#include <syscall.h>

void
__init(void *tls, size_t tls_size, void *start, void *end)
{
    __debug_putc(':');
    __debug_putc(')');
    while (1) ;
}