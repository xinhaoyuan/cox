#ifndef __USER_DEBUG_H__
#define __USER_DEBUG_H__

#include <stdarg.h>

void debug_printf(const char *fmt, ...);
void debug_vprintf(const char *fmt, va_list ap);

#endif
