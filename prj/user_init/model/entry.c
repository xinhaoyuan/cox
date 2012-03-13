#include <string.h>
#include <lib/low_io.h>
#include "runtime.h"

void
entry(tls_t __tls)
{
	thread_init(__tls);
	low_io_putc = debug_putchar;
	cprintf("Hello world\n");
	
	while (1) ;
}
