#ifndef __KERN_DEBUG_H__
#define __KERN_DEBUG_H__

#include <stdarg.h>
#include <asm/cpu.h>

#define DBG_NO     (0)
#define DBG_IO     (1 << 0)
#define DBG_SCHED  (1 << 1)
#define DBG_MEM    (1 << 2)
#define DBG_MISC   (1 << 3)

#ifndef __DEBUG_BITS
#define DEBUG_BITS (-1)
#else
#define DEBUG_BITS __DEBUG_BITS
#endif

#ifndef DEBUG_COMPONENT
#define DEBUG_COMPONENT DBG_NO
#endif

#define DEBUG(INFO ...) do {                    \
        __DEBUG_MSG(DEBUG_COMPONENT, INFO);     \
    } while (0)

#define __DEBUG_MSG(BITS,INFO ...) do {         \
        if ((DEBUG_BITS) & (BITS)) {            \
            debug_printf(INFO);                 \
        }                                       \
    } while (0)

#define PANIC(x ...) do { debug_printf("PANIC: " x); while (1) ; } while (0)

#define assert(x) do { if (!(x)) PANIC("assertion %s failed\n", #x); } while (0);

void debug_io_set(void(*putc)(int ch, void *priv),
                  int (*getc)(void *priv),
                  void *priv);
void debug_putc(int ch);
void debug_printf(const char *fmt, ...);
void debug_vprintf(const char *fmt, va_list ap);
int  debug_getc(void);
int  debug_readline(char *buf, int size);
        
#endif
