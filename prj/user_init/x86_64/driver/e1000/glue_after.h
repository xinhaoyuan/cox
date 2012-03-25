void
e1000_test(void)
{
	e1000_init_pci();
}

/* From e1000_writev_s / e1000_readv_s */

int
e1000_write(e1000_t *e, const void *buf, size_t buf_size)
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

int
e1000_read(e1000_t *e, void *buf, size_t buf_size)
{
    e1000_rx_desc_t *desc;
    int i, r, head, tail, cur, bytes = 0, size;

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

	return 0;
}
