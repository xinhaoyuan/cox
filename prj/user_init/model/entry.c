#include <string.h>
#include <lib/low_io.h>
#include <runtime/local.h>
#include <runtime/fiber.h>
#include <runtime/io.h>
#include <runtime/page.h>
#include <driver/pci/pci.h>
#include <driver/e1000/glue_inc.h>
#include <mach.h>

static char f1stack[4096];
static fiber_s f1;

static void
fiber1(void *arg)
{
#if 0
	io_data_s mmio = IO_DATA_INITIALIZER(2, IO_MMIO_OPEN, 0xB8000, 0x1000);
	io(&mmio, IO_MODE_SYNC);
	cprintf("%d %016lx\n", mmio.io[0], mmio.io[1]);
	short int *buf = (void *)mmio.io[1];
	memset(buf, 0, 0x1000);

	int i;
	for (i = 0; i < 18; i ++)
	{
		buf[i] = "I have control! :)"[i] | 0x0700;
	}
#endif

	e1000_test();
	
	while (1) ;
}

void
entry(void)
{
	char *buf = palloc(1);
	memset(buf, 0, __PGSIZE);
	
	fiber_init(&f1, fiber1, (void *)0x12345678, f1stack, 4096);

	while (1)
		fiber_schedule();
}
