#ifndef __LOW_IO_H__
#define __LOW_IO_H__

#include <types.h>
#include <stdarg.h>

/* IO interface that driver fills */
extern void (*low_io_putc)(int ch);
extern int  (*low_io_getc)(void);

int  vcprintf(const char *fmt, va_list ap);
int  cprintf(const char *fmt, ...);
void cputchar(int c);
int  cputs(const char *str);
int  cgetchar(void);

#define assert(x) do { if (!(x)) { cprintf("assertion %s failed\n", #x); while (1) ; } } while (0);

#endif
