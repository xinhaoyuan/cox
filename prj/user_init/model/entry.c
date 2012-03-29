#include <string.h>
#include <lib/low_io.h>
#include <runtime/local.h>
#include <runtime/fiber.h>
#include <runtime/io.h>
#include <runtime/page.h>
#include <driver/pci/pci.h>
#include <driver/e1000/glue_inc.h>
#include <mach.h>
#include <lib/marshal.h>

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
	int mbox_tx, mbox_ctl, nic;
	
	{
		io_data_s nic_open = IO_DATA_INITIALIZER(3, IO_NIC_OPEN);
		io(&nic_open, IO_MODE_SYNC);
		nic = nic_open.io[0];
		mbox_tx = nic_open.io[1];
		mbox_ctl = nic_open.io[2];
		cprintf("Get NIC ID %d TX MBOX %d CTL MBOX %d\n", nic, mbox_tx, mbox_ctl);
	}

	/* { */
	/* 	io_data_s nic_up = IO_DATA_INITIALIZER(1, IO_NIC_CTL, nic, IO_NIC_CTL_UP); */
	/* 	io(&nic_up, IO_MODE_SYNC); */
	/* } */


	{
		cprintf("Wait for syscall\n");
		io_data_s mbox_io = IO_DATA_INITIALIZER(5, IO_MBOX_IO, mbox_ctl, -1, 0, 0);
		io(&mbox_io, IO_MODE_SYNC);
		uintptr_t data;
		
		MARSHAL_DECLARE(buf, mbox_io.io[1], mbox_io.io[1] + 256);
		MARSHAL(buf, sizeof(uintptr_t), (data = 0, &data));
		MARSHAL(buf, sizeof(uintptr_t), (data = 1500, &data));
		MARSHAL(buf, sizeof(uintptr_t), (data = 6, &data));
		MARSHAL(buf, 6, "      ");
		MARSHAL(buf, sizeof(uintptr_t), (data = 4, &data));
		MARSHAL(buf, 4, "    ");
		MARSHAL(buf, sizeof(uintptr_t), (data = 4, &data));
		MARSHAL(buf, 4, "    ");
		MARSHAL(buf, sizeof(uintptr_t), (data = 4, &data));
		MARSHAL(buf, 4, "    ");
		
		mbox_io.io[0] = IO_MBOX_IO;
		mbox_io.io[1] = mbox_ctl;
		mbox_io.io[3] = MARSHAL_SIZE(buf);
		mbox_io.io[4] = NIC_CTL_CFG_SET;
		
		io(&mbox_io, IO_MODE_SYNC);
		
		mbox_io.io[0] = IO_MBOX_IO;
		mbox_io.io[1] = mbox_ctl;
		mbox_io.io[3] = 0;
		mbox_io.io[4] = NIC_CTL_ADD;

		io(&mbox_io, IO_MODE_SYNC);
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
