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

	e1000_test();
		
#endif

	io_data_s nic_open = IO_DATA_INITIALIZER(1, IO_NIC_OPEN);
	io(&nic_open, IO_MODE_SYNC);
	int nic = nic_open.io[0];
	cprintf("Get NIC ID %d\n", nic);
	
	io_data_s nic_req = IO_DATA_INITIALIZER(4, IO_NIC_NEXT_REQ_W, nic, -1);
	io(&nic_req, IO_MODE_SYNC);
	
	cprintf("Get NIC REQ W R = %d ID %d BUF %016lx SIZE %d\n", nic_req.io[0], nic_req.io[2], nic_req.io[1], nic_req.io[3]);
	cprintf("BUF: %s\n", (char *)nic_req.io[1]);

	while (1)
	{
		cprintf("Get more packet\n");
	
		nic_req.io[0] = IO_NIC_NEXT_REQ_W;
		nic_req.io[1] = nic;
		io(&nic_req, IO_MODE_SYNC);
		
		cprintf("Get NIC REQ W R = %d ID %d BUF %016lx SIZE %d\n", nic_req.io[0], nic_req.io[2], nic_req.io[1], nic_req.io[3]);
		cprintf("BUF: %s\n", (char *)nic_req.io[1]);
	}

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
