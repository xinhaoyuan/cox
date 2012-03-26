#include <nic.h>
#include <irq.h>
#include <spinlock.h>
#include <error.h>
#include <string.h>
#include <page.h>
#include <user.h>

static list_entry_s nic_free_list;
static spinlock_s   nic_alloc_lock;

static list_entry_s nic_req_io_free_list;
static spinlock_s   nic_req_io_alloc_lock;

nic_s nics[NICS_MAX_COUNT];
nic_req_io_s nic_reqs[NIC_REQS_MAX_COUNT];

static inline nic_t nic_get(int nic_id) __attribute__((always_inline));
static inline nic_t nic_get(int nic_id) { return nics + nic_id; }

static inline void nic_put(int nic_id) __attribute__((always_inline));
static inline void nic_put(int nic_id) { }

int
nic_init(void)
{
	list_init(&nic_free_list);
	list_init(&nic_req_io_free_list);

	spinlock_init(&nic_alloc_lock);
	spinlock_init(&nic_req_io_alloc_lock);

	int i;
	for (i = 0; i != NICS_MAX_COUNT; ++ i)
	{
		nics[i].status = NIC_STATUS_FREE;
		list_add_before(&nic_free_list, &nics[i].free_list);
	}

	for (i = 0; i != NIC_REQS_MAX_COUNT; ++ i)
	{
		nic_reqs[i].status = NIC_REQ_IO_STATUS_FREE;
		list_add_before(&nic_req_io_free_list, &nic_reqs[i].free_list);
	}

	return 0;
}


int
nic_alloc(user_proc_t proc)
{
	list_entry_t l = NULL;
	
	int irq = irq_save();
	spinlock_acquire(&nic_alloc_lock);

	if (!list_empty(&nic_free_list))
	{
		l = list_next(&nic_free_list);
		list_del(l);
	}

	spinlock_release(&nic_alloc_lock);
	irq_restore(irq);

	if (l)
	{
		nic_t nic = CONTAINER_OF(l, nic_s, free_list);

		nic->status = NIC_STATUS_CLOSED;
		nic->proc   = proc;
		
		list_init(&nic->req_r_list);
		spinlock_init(&nic->req_r_lock);
		semaphore_init(&nic->req_r_sem, 0);

		list_init(&nic->req_w_list);
		spinlock_init(&nic->req_w_lock);
		semaphore_init(&nic->req_w_sem, 0);

		return nic - nics;
	}
	else return -1;
}

void
nic_free(int nic_id)
{
	/* XXX */
}

int
nic_write_packet(int nic_id, const void *packet, size_t packet_size)
{
	nic_t nic = nic_get(nic_id);
	if (nic == NULL) return -E_INVAL;

	semaphore_acquire(&nic->req_w_sem, NULL);
	spinlock_acquire(&nic->req_w_lock);
	
	list_entry_t l = NULL;
	if (!list_empty(&nic->req_w_list))
	{
		l = list_next(&nic->req_w_list);
		list_del(l);
	}

	spinlock_release(&nic->req_w_lock);
	
	if (l == NULL)
	{
		nic_put(nic_id);
		return -E_NO_MEM;
	}

	ips_node_s ips;

	nic_req_io_t io = CONTAINER_OF(l, nic_req_io_s, req_list);
	IPS_NODE_PTR_SET(&ips, current);
	IPS_NODE_WAIT_SET(&ips);
	io->ips = &ips;
	io->status = NIC_REQ_IO_STATUS_PROCESSING;
	io_call_entry_t ce = &io->io_proc->user_thread->ioce.head[io->io_index];
	/* WRITE MESSAGE */
	if (packet_size > (NIC_REQ_IO_UBUF_PSIZE << __PGSHIFT))
		packet_size = (NIC_REQ_IO_UBUF_PSIZE << __PGSHIFT);
	ce->ce.data[0] = 0;
	ce->ce.data[1] = io->ubuf_u;
	ce->ce.data[2] = io - nic_reqs;
	ce->ce.data[3] = packet_size;
	io->buf = NULL; io->buf_size = 0;
	memmove(io->ubuf, packet, packet_size);
	user_thread_iocb_push(io->io_proc, io->io_index);

	while (IPS_NODE_WAIT(&ips))
		proc_wait_try();

	nic_put(nic_id);

	return 0;
}

int
nic_read_packet(int nic_id, void *buf, size_t buf_size)
{
	if (buf == NULL || buf_size == 0) return -E_INVAL;
	
	nic_t nic = nic_get(nic_id);
	if (nic == NULL) return -E_INVAL;

	semaphore_acquire(&nic->req_r_sem, NULL);
	spinlock_acquire(&nic->req_r_lock);
	
	list_entry_t l = NULL;
	if (!list_empty(&nic->req_r_list))
	{
		l = list_next(&nic->req_r_list);
		list_del(l);
	}

	spinlock_release(&nic->req_r_lock);

	if (l == NULL)
	{
		nic_put(nic_id);
		return -E_NO_MEM;
	}

	ips_node_s ips;
	
	nic_req_io_t io = CONTAINER_OF(l, nic_req_io_s, req_list);
	IPS_NODE_PTR_SET(&ips, current);
	IPS_NODE_WAIT_SET(&ips);
	io->ips = &ips;
	io->status = NIC_REQ_IO_STATUS_PROCESSING;
	io_call_entry_t ce = &io->io_proc->user_thread->ioce.head[io->io_index];
	if (buf_size > (NIC_REQ_IO_UBUF_PSIZE << __PGSHIFT))
		buf_size = (NIC_REQ_IO_UBUF_PSIZE << __PGSHIFT);
	/* WRITE MESSAGE */
	ce->ce.data[0] = 0;
	ce->ce.data[1] = io->ubuf_u;
	ce->ce.data[2] = io - nic_reqs;
	ce->ce.data[3] = buf_size;
	io->buf = buf;
	io->buf_size = buf_size;
	user_thread_iocb_push(io->io_proc, io->io_index);

	while (IPS_NODE_WAIT(&ips))
		proc_wait_try();

	nic_put(nic_id);

	return 0;
}

int
nic_req_io(int nic_id, int ack, size_t ack_size, proc_t io_proc, iobuf_index_t io_index, int op_w)
{
	cprintf("NIC REQ IO\n");
	int result;
	void *ubuf = NULL;
	uintptr_t ubuf_u;
	
	if (ack != -1 && nic_reqs[ack].status == NIC_REQ_IO_STATUS_PROCESSING)
	{
		if (nic_reqs[ack].buf != NULL)
		{
			/* if the ack is a read buf, copy read data into up level buffer */
			if (ack_size > nic_reqs[ack].buf_size) ack_size = nic_reqs[ack].buf_size;
			memmove(nic_reqs[ack].buf, nic_reqs[ack].ubuf, ack_size);
		}
		
		nic_reqs[ack].status = NIC_REQ_IO_STATUS_FINISHED;
		IPS_NODE_WAIT_CLEAR(nic_reqs[ack].ips);
		proc_notify(IPS_NODE_PTR(nic_reqs[ack].ips));
		
		ubuf = nic_reqs[ack].ubuf;
		ubuf_u = nic_reqs[ack].ubuf_u;
	}

	list_entry_t l = NULL;
	nic_t nic = nic_get(nic_id);

	if (nic == NULL)
	{
		result = -E_INVAL;
		goto nomore_io;
	}
	
	int irq = irq_save();
	spinlock_acquire(&nic_req_io_alloc_lock);

	/* free the ack req */
	if (ubuf != NULL)
	{
		list_add(&nic_req_io_free_list, &nic_reqs[ack].free_list);
	}

	if (!list_empty(&nic_req_io_free_list))
	{
		l = list_next(&nic_req_io_free_list);
		list_del(l);
	}

	spinlock_release(&nic_req_io_alloc_lock);
	irq_restore(irq);

	if (l == NULL)
	{
		result = -E_INVAL;
		goto nomore_io;
	}

	if (ubuf == NULL)
	{
		page_t p = page_alloc_atomic(NIC_REQ_IO_UBUF_PSIZE);
		if (p == NULL)
		{
			result = -E_INVAL;
			goto nomore_io;
		}
		
		ubuf = VADDR_DIRECT(PAGE_TO_PHYS(p));
		
		if (user_proc_arch_mmio_open(io_proc->user_proc, PADDR_DIRECT(ubuf), NIC_REQ_IO_UBUF_PSIZE << __PGSHIFT, &ubuf_u))
		{
			result = -E_INVAL;
			goto nomore_io;
		}
	}
	else nic_put(nic_id);

	nic_req_io_t req = CONTAINER_OF(l, nic_req_io_s, free_list);
	req->status   = NIC_REQ_IO_STATUS_QUEUEING;
	req->ips      = NULL;
	req->io_proc  = io_proc;
	req->io_index = io_index;
	req->nic      = nic;
	req->ubuf     = ubuf;
	req->ubuf_u   = ubuf_u;

	if (op_w)
	{
		spinlock_acquire(&nic->req_w_lock);
		list_add(&nic->req_w_list, &req->req_list);
		spinlock_release(&nic->req_w_lock);
		semaphore_release(&nic->req_w_sem);
	}
	else
	{
		spinlock_acquire(&nic->req_r_lock);
		list_add(&nic->req_r_list, &req->req_list);
		spinlock_release(&nic->req_r_lock);
		semaphore_release(&nic->req_r_sem);
	}
	
	return 0;

  nomore_io:
	if (ubuf != NULL)
	{
		page_free_atomic(PHYS_TO_PAGE(PADDR_DIRECT(ubuf)));
		if (nic) nic_put(nic_id);
	}
	if (result) cprintf("NIC REQ IO FAILED\n");
	return result;
}
