#include <nic.h>
#include <irq.h>
#include <spinlock.h>
#include <error.h>

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

	nic_req_io_t io = CONTAINER_OF(l, nic_req_io_s, req_list);

	io->status = NIC_REQ_IO_STATUS_PROCESSING;
	/* WRITE MESSAGE */
	//iocb_push(io->io_proc, io->io_index);

	while (io->status != NIC_REQ_IO_STATUS_FINISHED)
		proc_wait_try();

	/* release the io request */
	spinlock_acquire(&nic_req_io_alloc_lock);
	list_add(&nic_req_io_free_list, &io->free_list);
	spinlock_release(&nic_req_io_alloc_lock);
	nic_put(nic_id);


	nic_put(nic_id);

	return 0;
}

int
nic_read_packet(int nic_id, void *buf, size_t buf_size)
{
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

	nic_req_io_t io = CONTAINER_OF(l, nic_req_io_s, req_list);

	io->status = NIC_REQ_IO_STATUS_PROCESSING;
	/* WRITE MESSAGE */
	//iocb_push(io->io_proc, io->io_index);

	while (io->status != NIC_REQ_IO_STATUS_FINISHED)
		proc_wait_try();

	/* release the io request */
	spinlock_acquire(&nic_req_io_alloc_lock);
	list_add(&nic_req_io_free_list, &io->free_list);
	spinlock_release(&nic_req_io_alloc_lock);
	nic_put(nic_id);


	nic_put(nic_id);

	return 0;
}

int
nic_read_io_wait(int nic_id, proc_t io_proc, iobuf_index_t io_index)
{
	list_entry_t l = NULL;
	nic_t nic = nic_get(nic_id);

	if (nic == NULL) return -E_INVAL;
	
	int irq = irq_save();
	spinlock_acquire(&nic_req_io_alloc_lock);

	if (!list_empty(&nic_req_io_free_list))
	{
		l = list_next(&nic_req_io_free_list);
		list_del(l);
	}

	spinlock_release(&nic_req_io_alloc_lock);
	irq_restore(irq);

	if (l)
	{
		nic_req_io_t req = CONTAINER_OF(l, nic_req_io_s, free_list);
		req->status   = NIC_REQ_IO_STATUS_QUEUEING;
		req->req_proc = NULL;
		req->io_proc  = io_proc;
		req->io_index = io_index;
		req->nic      = nic;
		
		spinlock_acquire(&nic->req_r_lock);
		list_add(&nic->req_r_list, &req->req_list);
		spinlock_release(&nic->req_r_lock);
		
		semaphore_release(&nic->req_r_sem);
		return 0;
	}
	else return -E_NO_MEM;
}

int
nic_write_io_wait(int nic_id, proc_t io_proc, iobuf_index_t io_index)
{
	list_entry_t l = NULL;
	nic_t nic = nic_get(nic_id);

	if (nic == NULL) return -E_INVAL;
	
	int irq = irq_save();
	spinlock_acquire(&nic_req_io_alloc_lock);

	if (!list_empty(&nic_req_io_free_list))
	{
		l = list_next(&nic_req_io_free_list);
		list_del(l);
	}

	spinlock_release(&nic_req_io_alloc_lock);
	irq_restore(irq);

	if (l)
	{
		nic_req_io_t req = CONTAINER_OF(l, nic_req_io_s, free_list);
		req->status   = NIC_REQ_IO_STATUS_QUEUEING;
		req->req_proc = NULL;
		req->io_proc  = io_proc;
		req->io_index = io_index;
		req->nic      = nic;
		
		spinlock_acquire(&nic->req_w_lock);
		list_add(&nic->req_w_list, &req->req_list);
		spinlock_release(&nic->req_w_lock);
		
		semaphore_release(&nic->req_w_sem);
		return 0;
	}
	else return -E_NO_MEM;
}

void
nic_reply_request(int req)
{
	if (nic_reqs[req].status != NIC_REQ_IO_STATUS_PROCESSING) return;
	nic_reqs[req].status = NIC_REQ_IO_STATUS_FINISHED;
	proc_notify(nic_reqs[req].req_proc);
}
