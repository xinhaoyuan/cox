#include <runtime/sync.h>
#include <runtime/fiber.h>
#include <lib/marshal.h>

e1000_t e1000_sample;

static io_data_s io_tx, io_rx, io_ctl;
static int nic, mbox_tx, mbox_rx, mbox_ctl;
static semaphore_s sem_tx;
static fiber_s e1000_if, e1000_tf;
static char e1000_if_stack[10240] __attribute__((aligned(__PGSIZE)));
static char e1000_tf_stack[10240] __attribute__((aligned(__PGSIZE)));

static void e1000_interrupt_fiber(void *__data);
static void e1000_tx_fiber(void *__data);

void
e1000_test(void)
{
    e1000_t *e = &e1000_sample;
    
    pci_init();

    strcpy(e->name, "e1000#0");
    e->name[6] += e1000_instance;   
    e1000_probe(e, e1000_instance);

    if (!(e->status & E1000_ENABLED) && !(e1000_init_hw(e)))
    {
        cprintf("e1000_test init FAILED\n");
    }

    {
        io_data_s nic_open = IO_DATA_INITIALIZER(4, IO_NIC_OPEN);
        io(&nic_open, IO_MODE_SYNC);
        nic = nic_open.io[0];
        mbox_tx = nic_open.io[1];
        mbox_rx = nic_open.io[2];
        mbox_ctl = nic_open.io[3];
    }
    
    mbox_io_begin(&io_tx);
    mbox_io_begin(&io_rx);
    mbox_io_begin(&io_ctl);

    semaphore_init(&sem_tx, 0);

    /* Start recv by active io */
    mbox_io_recv(&io_rx, IO_MODE_SYNC, mbox_rx, 0, 0);

    fiber_init(&e1000_if, e1000_interrupt_fiber, e, e1000_if_stack, 10240);
    fiber_init(&e1000_tf, e1000_tx_fiber, e, e1000_tf_stack, 10240);

    mbox_io_recv(&io_ctl, IO_MODE_SYNC, mbox_ctl, 0, 0);
    
    uintptr_t data;
    MARSHAL_DECLARE(buf, io_ctl.io[1], io_ctl.io[1] + 256);

    MARSHAL(buf, sizeof(uintptr_t), (data = 0, &data));
    MARSHAL(buf, sizeof(uintptr_t), (data = 1500, &data));
    MARSHAL(buf, sizeof(uintptr_t), (data = 6, &data));
    MARSHAL(buf, 6, &e->address.ea_addr[0]);
    MARSHAL(buf, sizeof(uintptr_t), (data = 4, &data));
    MARSHAL(buf, 4, "\xa\x0\x2\xf");
    MARSHAL(buf, sizeof(uintptr_t), (data = 4, &data));
    MARSHAL(buf, 4, "\xff\xff\xff\x0");
    MARSHAL(buf, sizeof(uintptr_t), (data = 4, &data));
    MARSHAL(buf, 4, "\xa\x0\x2\x2");
    mbox_io_recv(&io_ctl, IO_MODE_SYNC, mbox_ctl, MARSHAL_SIZE(buf), NIC_CTL_CFG_SET);

    mbox_io_recv(&io_ctl, IO_MODE_SYNC, mbox_ctl, 0, NIC_CTL_ADD);
    
    mbox_io_recv(&io_ctl, IO_MODE_SYNC, mbox_ctl, 0, NIC_CTL_UP);
}

/* From e1000_writev_s / e1000_readv_s */

int
e1000_tx_packet(e1000_t *e, const void *buf, size_t buf_size)
{
    e1000_tx_desc_t *desc;
    int r, head, tail, i, bytes = 0, size;

    E1000_DEBUG(3, ("e1000: write(%p,%d)\n", buf, buf_size));

    /* We cannot write twice simultaneously. 
       assert(!(e->status & E1000_WRITING)); */
    
    e->status |= E1000_WRITING;

    /* Find the head, tail and current descriptors. */
    head =  e1000_reg_read(e, E1000_REG_TDH);
    tail =  e1000_reg_read(e, E1000_REG_TDT);
    desc = &e->tx_desc[tail];
    
    E1000_DEBUG(4, ("%s: head=%d, tail=%d\n",
                    e->name, head, tail));

    while (buf_size > 0)
    {
        size      = buf_size > E1000_IOBUF_SIZE ? E1000_IOBUF_SIZE : buf_size;
        buf_size -= size;

        memmove(e->tx_buffer + (tail * E1000_IOBUF_SIZE), buf, size);
        desc->status  = 0;
        desc->command = 0;
        desc->length  = size;

        /* Marks End-of-Packet. */
        if (buf_size == 0)
        {
            desc->command = E1000_TX_CMD_EOP |
                E1000_TX_CMD_FCS |
                E1000_TX_CMD_RS;
        }

        tail   = (tail + 1) % e->tx_desc_count;
        bytes += size;
        buf   += size;
        desc   = &e->tx_desc[tail];
    }

    /* Increment tail. Start transmission. */
    e1000_reg_write(e, E1000_REG_TDT, tail);

    E1000_DEBUG(2, ("e1000: wrote %d byte packet\n", bytes));

    return 0;
}

static int
e1000_rx_packet(e1000_t *e, void *buf, size_t buf_size, size_t *rx_size)
{
    e1000_rx_desc_t *desc;
    int head, tail, cur, bytes = 0, size;

    E1000_DEBUG(3, ("e1000: read(%p,%d)\n", buf, buf_size));

    /* Find the head, tail and current descriptors. */
    head = e1000_reg_read(e, E1000_REG_RDH);
    tail = e1000_reg_read(e, E1000_REG_RDT);
    cur  = (tail + 1) % e->rx_desc_count;
    desc = &e->rx_desc[cur];
    
    /*
     * Only handle one packet at a time.
     */
    if (!(desc->status & E1000_RX_STATUS_EOP))
    {
        return -E_BUSY;
    }
    E1000_DEBUG(4, ("%s: head=%x, tail=%d\n",
                    e->name, head, tail));

    size = desc->length < buf_size ? desc->length : buf_size;
    memmove(buf, e->rx_buffer + (cur * E1000_IOBUF_SIZE), size);
    desc->status = 0;

    /*
     * Update state.
     */
    e->status   |= E1000_RECEIVED;
    E1000_DEBUG(2, ("e1000: got %d byte packet\n", size));

    /* Increment tail. */
    e1000_reg_write(e, E1000_REG_RDT, (tail + 1) % e->rx_desc_count);
    *rx_size = size;

    return 0;
}

static void
e1000_rx(e1000_t *e)
{
    cprintf("e1000_rx begin!\n");
    size_t rx_size = 0;
    while (1)
    {
        void *buf = io_rx.io[1];
        if (e1000_rx_packet(e, buf,  __PGSIZE, &rx_size) < 0)
            break;
        mbox_io_recv(&io_rx, IO_MODE_SYNC, mbox_rx, rx_size, 0);
    }
    cprintf("e1000_rx end!\n");
}

static void
e1000_tx_fiber(void *__data)
{
    e1000_t *e = (e1000_t *)__data;
    
    while (1)
    {
        cprintf("wait for tx request\n");
        mbox_io_recv(&io_tx, IO_MODE_SYNC, mbox_tx, 0, 0);
        void *buf = io_tx.io[1];
        size_t buf_size = io_tx.io[2]; 

        while (1)
        {
            cprintf("e1000_tx try\n");
            if (e1000_tx_packet(e, buf, buf_size) == 0) break;
            cprintf("failed, wait for sem\n");
            semaphore_acquire(&sem_tx, NULL);
        }
    }
}

static void
e1000_interrupt_fiber(void *__data)
{
    e1000_t *e = (e1000_t *)__data;
    u32_t cause;

    int mbox_irq;

    {
        io_data_s mbox_open = IO_DATA_INITIALIZER(1, IO_MBOX_OPEN);
        io(&mbox_open, IO_MODE_SYNC);
        mbox_irq = mbox_open.io[0];
    }

    {
        io_data_s irq_listen = IO_DATA_INITIALIZER(1, IO_IRQ_LISTEN, e->irq, mbox_irq);
        io(&irq_listen, IO_MODE_SYNC);
        if (irq_listen.io[0]) cprintf("irq listen failed\n");
        else cprintf("listen irq %d\n", e->irq);
    }

    io_data_s mbox_io;
    mbox_io_begin(&mbox_io);
    
    while (1)
    {
        E1000_DEBUG(3, ("e1000: interrupt\n"));
                
        /* Read the Interrupt Cause Read register. */
        if ((cause = e1000_reg_read(e, E1000_REG_ICR)))
        {
            if (cause & E1000_REG_ICR_LSC)
            {
                cprintf("XXX: e1000 link changed\n");
            }
            
            if (cause & (E1000_REG_ICR_RXO | E1000_REG_ICR_RXT))
                e1000_rx(e);
            
            if ((cause & E1000_REG_ICR_TXQE) ||
                (cause & E1000_REG_ICR_TXDW))
                semaphore_release(&sem_tx);
        }

        mbox_io_recv(&mbox_io, IO_MODE_SYNC, mbox_irq, 0, 0);
    }

    mbox_io_end(&mbox_io);
}
