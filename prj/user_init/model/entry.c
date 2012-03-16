#include <string.h>
#include <lib/low_io.h>
#include "runtime/local.h"
#include "runtime/fiber.h"
#include "runtime/io.h"

static char f1stack[4096];
static fiber_s f1;

static void
fiber1(void *arg)
{
	cprintf("Hello world from fiber 1, arg = %016lx\n", arg);
	while (1) ;
}

void
entry(tls_t __tls)
{
	thread_init(__tls);
	low_io_putc = debug_putchar;
	
	fiber_init(&f1, fiber1, (void *)0x12345678, f1stack, 4096);

	while (1)
		fiber_schedule();
}
