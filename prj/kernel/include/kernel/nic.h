#ifndef __KERN_NIC_H__
#define __KERN_NIC_H__

#include <types.h>
#include <user.h>
#include <user/io.h>
#include <algo/list.h>
#include <lwip/netif.h>

struct nic_ctl_s
{
	list_entry_s ctl_list;
	
	uintptr_t func;
	char buf[NIC_CTL_BUF_SIZE];
};

#define NIC_CFG_BUF_SIZE 256

struct nic_s
{
	union
	{
		list_entry_s free_list;
		list_entry_s nic_list;
	};

	user_proc_t proc;
	int  status;
	int  mbox_tx;
	int  mbox_ctl;

	char cfg_buf[NIC_CFG_BUF_SIZE];

	struct nic_ctl_s ctl;
	struct netif netif;
};

typedef struct nic_s nic_s;
typedef nic_s *nic_t;

#define NIC_STATUS_FREE     0
#define NIC_STATUS_UNINIT   1
#define NIC_STATUS_DETACHED 2
#define NIC_STATUS_ATTACHED 3

#define NICS_MAX_COUNT 256

extern nic_s nics[NICS_MAX_COUNT];

int  nic_init(void);

int  nic_alloc(user_proc_t proc, int *mbox_tx, int *mbox_ctl);
void nic_free(int nic_id);
void nic_status_set(int nic_id, int status);
/* Submit packet into network stack */
void nic_input(int nic_id, void *packet, size_t packet_size);

#endif
