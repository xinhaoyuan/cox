#ifndef __KERN_NIC_H__
#define __KERN_NIC_H__

#include <user.h>
#include <ips.h>
#include <algo/list.h>
#include <user/io.h>

struct nic_s
{
	union
	{
		list_entry_s proc_list;
		list_entry_s free_list;
	};

	int status;
	user_proc_t proc;
	
	semaphore_s  req_w_sem;
	spinlock_s   req_w_lock;
	list_entry_s req_w_list;
	
	
	semaphore_s  req_r_sem;
	spinlock_s   req_r_lock;
	list_entry_s req_r_list;
};

typedef struct nic_s nic_s;
typedef nic_s *nic_t;

struct nic_req_io_s
{
	union
	{
		list_entry_s  req_list;
		list_entry_s  free_list;
	};

	proc_t        req_proc;
	proc_t        io_proc;
	int           status;
	nic_t         nic;
	iobuf_index_t io_index;
};

#define NIC_STATUS_FREE   0
#define NIC_STATUS_CLOSED 1
#define NIC_STATUS_OPEN   2

#define NIC_REQ_IO_STATUS_FREE       0
#define NIC_REQ_IO_STATUS_QUEUEING   1
#define NIC_REQ_IO_STATUS_PROCESSING 2
#define NIC_REQ_IO_STATUS_FINISHED   3

typedef struct nic_req_io_s nic_req_io_s;
typedef nic_req_io_s *nic_req_io_t;

#define NICS_MAX_COUNT     256
#define NIC_REQS_MAX_COUNT 4096

extern nic_s nics[NICS_MAX_COUNT];
extern nic_req_io_s nic_reqs[NIC_REQS_MAX_COUNT];

int  nic_alloc(user_proc_t proc);
void nic_free(int nic_id);
int  nic_write_packet(int nic_id, const void *packet, size_t packet_size);
int  nic_read_packet(int nic_id, void *buf, size_t buf_size);
int  nic_read_io_wait(int nic_id, proc_t io_proc, iobuf_index_t io_index);
int  nic_write_io_wait(int nic_id, proc_t io_proc, iobuf_index_t io_index);
void nic_reply_request(int req);

#endif
