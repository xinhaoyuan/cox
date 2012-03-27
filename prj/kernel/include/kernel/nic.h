#ifndef __KERN_NIC_H__
#define __KERN_NIC_H__

#include <types.h>
#include <user.h>
#include <algo/list.h>

struct nic_s
{
	union
	{
		list_entry_s free_list;
		list_entry_s nic_list;
	};

	user_proc_t proc;
	int  status;
	int  mbox_w;

	struct netif *netif;
};

typedef struct nic_s nic_s;
typedef nic_s *nic_t;

#define NIC_STATUS_FREE   0
#define NIC_STATUS_UNINIT 1
#define NIC_STATUS_UP     2
#define NIC_STATUS_DOWN   3

#define NICS_MAX_COUNT 256

extern nic_s nics[NICS_MAX_COUNT];

int  nic_init(void);

int  nic_alloc(user_proc_t proc, int mbox_w);
void nic_free(int nic_id);
void nic_status_set(int nic_id, int status);
void nic_mac_set(int nic_id, void *mac_buf);
void nic_input(int nic_id, void *packet, size_t packet_size);

#endif
