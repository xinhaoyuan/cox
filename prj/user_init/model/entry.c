#include <string.h>
#include <lib/low_io.h>
#include <runtime/local.h>
#include <runtime/fiber.h>
#include <runtime/io.h>
#include <runtime/page.h>
/* #include <driver/pci/pci.h> */
/* #include <driver/e1000/glue_inc.h> */
#include <mach.h>
#include <lib/marshal.h>
#include "kbdreg.h"
#include <io.h>

static char f1stack[4096] __attribute__((aligned(__PGSIZE)));
static fiber_s f1;

#if 0

static char f2stack[4096] __attribute__((aligned(__PGSIZE)));
static fiber_s f2;
int mbox;

static void
fiber2(void *arg)
{
    io_data_s mbox_io;
    io_call_entry_t ce = mbox_io_begin(&mbox_io);
    mbox_io_attach(&mbox_io, mbox, 1, __PGSIZE);
    if (ce->data[0] != 0)
    {
        cprintf("error\n");
    }
    void *buf = (void *)ce->data[1];
        
    while (1)
    {
        strcpy(buf, "HELLO WORLD FROM MBOX");
        ce->data[1] = MBOX_IOBUF_POLICY_DO_MAP;
        ce->data[2] = 0x1234;
        ce->data[3] = 0x4567;
        mbox_do_io(&mbox_io, IO_MODE_SYNC);
    }
}

#endif

static void
kbd_proc_data(void)
{
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

#if 1
    {
        io_data_s sleep = IO_DATA_INITIALIZER(0, IO_SLEEP, TICK + 200);
        io(&sleep, IO_MODE_SYNC);
    }

    cprintf("hello again\n");
#endif
    

#if 0
    int mbox;

    {
        io_data_s mbox_open = IO_DATA_INITIALIZER(1, IO_MBOX_OPEN);
        io(&mbox_open, IO_MODE_SYNC);
        mbox = mbox_open.io[0];
        cprintf("got mbox %d\n", mbox);
    }

    {
        io_data_s irq_listen = IO_DATA_INITIALIZER(1, IO_IRQ_LISTEN, 1, mbox);
        io(&irq_listen, IO_MODE_SYNC);
        if (irq_listen.io[0]) cprintf("irq listen failed\n");
    }

    kbd_proc_data();
    {
        io_data_s mbox_io;
        io_call_entry_t ce = mbox_io_begin(&mbox_io);
        mbox_io_attach(&mbox_io, mbox, 0, 0);
        
        int count = 0;
        while (1)
        {
            cprintf("TO RECV\n");
            mbox_do_io(&mbox_io, IO_MODE_SYNC);
            cprintf("%d\n", count ++ );
            kbd_proc_data();
        }
    }

#endif

#if 0
    {
        io_data_s mbox_open = IO_DATA_INITIALIZER(1, IO_MBOX_OPEN);
        io(&mbox_open, IO_MODE_SYNC);
        mbox = mbox_open.io[0];
    }

    fiber_init(&f2, fiber2, (void *)0x12345678, f2stack, 4096);

    io_data_s mbox_io;
    io_call_entry_t ce = mbox_io_begin(&mbox_io);
    mbox_io_attach(&mbox_io, mbox, 0, 0);

    while (1)
    {
        mbox_do_io(&mbox_io, IO_MODE_SYNC);
        cprintf("GET IO HINT (%016lx, %016lx) BUF: %s\n", ce->data[2], ce->data[3], ce->data[1]);
    }

    
#endif

    while (1) fiber_schedule();
}

void
entry(void)
{
    /* char *buf = palloc(1); */
    /* memset(buf, 0, __PGSIZE); */
    
    fiber_init(&f1, fiber1, (void *)0x12345678, f1stack, 4096);

    /* cprintf("Hello world\n"); */
}
