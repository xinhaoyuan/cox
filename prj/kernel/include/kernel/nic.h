#ifndef __KERN_NIC_H__
#define __KERN_NIC_H__

#include <user.h>
#include <ips.h>
#include <algo/list.h>
#include <user/io.h>

typedef size_t nic_id_t;

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
	mutex_s      req_w_lock;
	list_entry_s req_w_list;
	
	
	semaphore_s  req_r_sem;
	mutex_s      req_r_lock;
	list_entry_s req_r_list;
};

typedef struct nic_s nic_s;
typedef nic_s *nic_t;

struct req_wait_io_s
{
	list_entry_s  req_list;
	proc_t        proc;
	iobuf_index_t index;
};

typedef struct req_wait_io_s req_wait_io_s;
typedef req_wait_io_s *req_wait_io_t;

#define NICS_MAX_COUNT 256
extern nic_s nics[NICS_MAX_COUNT];

int  nic_alloc(void);
void nic_free(int nic);
int  nic_write_packet(int nic, const void *packet, size_t packet_size);
int  nic_read_packet(int nic, void *buf, size_t buf_size);

#endif
