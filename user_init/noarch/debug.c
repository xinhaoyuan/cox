#include <arch/service.h>
#include <lib/printfmt.h>

#include "debug.h"

static void
__putc(int ch, void *priv)
{
    __service_sys2(SERVICE_SYS_DEBUG_PUTC, ch);
}

void
debug_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    debug_vprintf(fmt, ap);
    va_end(ap);
}

void
debug_vprintf(const char *fmt, va_list ap)
{
    vprintfmt(__putc, NULL, fmt, ap);
}
