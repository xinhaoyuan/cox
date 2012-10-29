#include <debug.h>
#include <lib/printfmt.h>
#include <spinlock.h>

static void (*io_putc) (int ch, void *priv);
static int  (*io_getc) (void *priv);
static void *io_priv;

static spinlock_s debug_lock = SPINLOCK_UNLOCKED_INITIALIZER;

void
debug_io_set(void(*putc)(int ch, void *priv),
             int (*getc)(void *priv),
             void *priv)
{
    spinlock_acquire(&debug_lock);
    io_putc = putc;
    io_getc = getc;
    io_priv = priv;
    spinlock_release(&debug_lock);
}

int
debug_getc(void)
{
    int r = -1;
    spinlock_acquire(&debug_lock);
    if (io_getc)
        r = io_getc(io_priv);
    spinlock_release(&debug_lock);
    return r;
}

void
debug_putc(int ch)
{
    spinlock_acquire(&debug_lock);
    if (io_putc)
        io_putc(ch, io_priv);
    spinlock_release(&debug_lock);
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
    spinlock_acquire(&debug_lock);
    if (io_putc)
        vprintfmt(io_putc, io_priv, fmt, ap);
    spinlock_release(&debug_lock);
}
