#ifndef __PRINTFMT_H__
#define __PRINTFMT_H__

#include <stdarg.h>

void printfmt(void (*putch)(int, void *), void *putdat, const char *fmt, ...);
void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap);
int  snprintf(char *str, size_t size, const char *fmt, ...);
int  vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

#endif
