#ifndef __IO_H__
#define __IO_H__

#include "sync.h"
#include "local.h"

void __iocb(void *ret);

inline static int __io_save(void) __attribute__((always_inline));
inline static void __io_restore(int status) __attribute__((always_inline));

inline static int
__io_save(void)
{ int r = __io_ubusy; __io_ubusy_set(1); return r; }

inline static void
__io_restore(int status)
{
	if (status == 0)
	{
		__io_ubusy_set(0);
		if (__iocb_busy)
			__iocb(NULL);
	}
	else __io_ubusy_set(1);
}

#define IO(v ...) ((uintptr_t[]){v})

void io_init(void);
int  io_b(ips_node_t node, int argc, uintptr_t args[]);

void debug_putchar(int ch);

#endif
