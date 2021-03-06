#include <types.h>
#include <asm/io.h>

#include "intr.h"
#include "pic.h"

// I/O Addresses of the two programmable interrupt controllers
#define IO_PIC1             0x20    // Master (IRQs 0-7)
#define IO_PIC2             0xA0    // Slave (IRQs 8-15)

#define IRQ_SLAVE           2       // IRQ at which slave connects to master

// Current IRQ mask.
// Initial IRQ mask has interrupt 2 enabled (for slave 8259A).
static uint16_t irq_mask = 0xFFFF & ~(1 << IRQ_SLAVE);
static bool did_init = 0;

static void
pic_setmask(uint16_t mask) {
    irq_mask = mask;
    if (did_init) {
        __outb(IO_PIC1 + 1, mask);
        __outb(IO_PIC2 + 1, mask >> 8);
    }
}

void
pic_enable(unsigned int irq) {
    pic_setmask(irq_mask & ~(1 << irq));
}

/* pic_init - initialize the 8259A interrupt controllers */
int
pic_init(void) {
    if (did_init) return 0;
    did_init = 1;

    // mask all interrupts
    __outb(IO_PIC1 + 1, 0xFF);
    __outb(IO_PIC2 + 1, 0xFF);

    // Set up master (8259A-1)

    // ICW1:  0001g0hi
    //    g:  0 = edge triggering, 1 = level triggering
    //    h:  0 = cascaded PICs, 1 = master only
    //    i:  0 = no ICW4, 1 = ICW4 required
    __outb(IO_PIC1, 0x11);

    // ICW2:  Vector offset
    __outb(IO_PIC1 + 1, IRQ_OFFSET);

    // ICW3:  (master PIC) bit mask of IR lines connected to slaves
    //        (slave PIC) 3-bit # of slave's connection to master
    __outb(IO_PIC1 + 1, 1 << IRQ_SLAVE);

    // ICW4:  000nbmap
    //    n:  1 = special fully nested mode
    //    b:  1 = buffered mode
    //    m:  0 = slave PIC, 1 = master PIC
    //        (ignored when b is 0, as the master/slave role
    //         can be hardwired).
    //    a:  1 = Automatic EOI mode
    //    p:  0 = MCS-80/85 mode, 1 = intel x86 mode
    __outb(IO_PIC1 + 1, 0x1);

    // Set up slave (8259A-2)
    __outb(IO_PIC2, 0x11);    // ICW1
    __outb(IO_PIC2 + 1, IRQ_OFFSET + 8);  // ICW2
    __outb(IO_PIC2 + 1, IRQ_SLAVE);   // ICW3
    // NB Automatic EOI mode doesn't tend to work on the slave.
    // Linux source code says it's "to be investigated".
    __outb(IO_PIC2 + 1, 0x1); // ICW4

    // OCW3:  0ef01prs
    //   ef:  0x = NOP, 10 = clear specific mask, 11 = set specific mask
    //    p:  0 = no polling, 1 = polling mode
    //   rs:  0x = NOP, 10 = read IRR, 11 = read ISR
    __outb(IO_PIC1, 0x68);    // clear specific mask
    __outb(IO_PIC1, 0x0a);    // read IRR by default

    __outb(IO_PIC2, 0x68);    // OCW3
    __outb(IO_PIC2, 0x0a);    // OCW3

    /* if (irq_mask != 0xFFFF) { */
    /*     pic_setmask(irq_mask); */
    /* } */
    pic_setmask(0xffff);

    /* Disable PIT by Magic */
    __outb(0x43, 0x30);
    __outb(0x40, 0);
    __outb(0x40, 0);

    return 0;
}

