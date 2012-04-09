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
#include "kbdreg.h"
#include <io.h>

static char f1stack[4096] __attribute__((aligned(__PGSIZE)));
static char f2stack[4096] __attribute__((aligned(__PGSIZE)));
static fiber_s f1;
static fiber_s f2;

int mbox;

static void
fiber2(void *arg)
{
    io_data_s mbox_io;
    mbox_io_begin(&mbox_io);
    mbox_io_send(&mbox_io, IO_MODE_SYNC, -1, 0, 0);
        
    while (1)
    {
        strcpy((char *)mbox_io.io[1], "HELLO WORLD FROM MBOX");
        mbox_io_send(&mbox_io, IO_MODE_SYNC, mbox, 0x1234, 0x4567);
    }
}

static void
kbd_proc_data(void) {
    uint8_t data;

    if ((__inb(KBSTATP) & KBS_DIB) == 0) {
        return;
    }

    data = __inb(KBDATAP);
    cprintf("%ld KB get data %02x\n", TICK, data);
}

static void
fiber1(void *arg)
{
    cprintf("Hello curel world\n");

    // e1000_test();
        
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

#if 0
    {
        io_data_s sleep = IO_DATA_INITIALIZER(0, IO_SLEEP, TICK + 200);
        io(&sleep, IO_MODE_SYNC);
    }

    cprintf("hello again\n");
    
    int mbox_tx, mbox_rx, mbox_ctl, nic;
    
    {
        io_data_s nic_open = IO_DATA_INITIALIZER(4, IO_NIC_OPEN);
        io(&nic_open, IO_MODE_SYNC);
        nic = nic_open.io[0];
        mbox_tx = nic_open.io[1];
        mbox_rx = nic_open.io[2];
        mbox_ctl = nic_open.io[3];
    }

    /* { */
    /*  io_data_s nic_up = IO_DATA_INITIALIZER(1, IO_NIC_CTL, nic, IO_NIC_CTL_UP); */
    /*  io(&nic_up, IO_MODE_SYNC); */
    /* } */


    {
        cprintf("Wait for syscall\n");
        io_data_s mbox_io;
        mbox_io_begin(&mbox_io);
        mbox_io_recv(&mbox_io, IO_MODE_SYNC, mbox_ctl, 0, 0);
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

        mbox_io_recv(&mbox_io, IO_MODE_SYNC, mbox_ctl, MARSHAL_SIZE(buf), NIC_CTL_CFG_SET);
        
        mbox_io.io[0] = IO_MBOX_IO;
        mbox_io.io[1] = mbox_ctl;
        mbox_io.io[2] = 0;
        mbox_io.io[3] = NIC_CTL_ADD;

        mbox_io_recv(&mbox_io, IO_MODE_SYNC, mbox_ctl, 0, NIC_CTL_ADD);
    }

    {
        io_data_s rx_io;
        mbox_io_begin(&rx_io);
        mbox_io_recv(&rx_io, IO_MODE_SYNC, mbox_rx, 0, 0);
        mbox_io_recv(&rx_io, IO_MODE_SYNC, mbox_rx, 1234, 0);
        mbox_io_recv(&rx_io, IO_MODE_SYNC, mbox_rx, 2345, 0);
        mbox_io_recv(&rx_io, IO_MODE_SYNC, mbox_rx, 5678, 0);
    }


#endif

#if 0
    int mbox;
    
    {
        io_data_s mbox_open = IO_DATA_INITIALIZER(1, IO_MBOX_OPEN);
        io(&mbox_open, IO_MODE_SYNC);
        mbox = mbox_open.io[0];
    }

    {
        io_data_s irq_listen = IO_DATA_INITIALIZER(1, IO_IRQ_LISTEN, 1, mbox);
        io(&irq_listen, IO_MODE_SYNC);
        if (irq_listen.io[0]) cprintf("irq listen failed\n");
    }

    kbd_proc_data();
    {
        io_data_s mbox_io;
        mbox_io_begin(&mbox_io);
        int count = 0;
        while (1)
        {
            mbox_io_recv(&mbox_io, IO_MODE_SYNC, mbox, 0, 0);
            cprintf("%d\n", count ++ );
            kbd_proc_data();

            mbox_io.io[0] = IO_MBOX_IO;
            mbox_io.io[1] = mbox;
        }
    }

#endif

#if 1
    {
        io_data_s mbox_open = IO_DATA_INITIALIZER(1, IO_MBOX_OPEN);
        io(&mbox_open, IO_MODE_SYNC);
        mbox = mbox_open.io[0];
    }

    fiber_init(&f2, fiber2, (void *)0x12345678, f2stack, 4096);

    io_data_s mbox_io;
    mbox_io_begin(&mbox_io);

    while (1)
    {
        mbox_io_recv(&mbox_io, IO_MODE_SYNC, mbox, 0, 0);
        cprintf("GET IO HINT (%016lx, %016lx) BUF: %s\n", mbox_io.io[2], mbox_io.io[3], mbox_io.io[1]);
    }

    
#endif
    
    while (1) fiber_schedule();
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
