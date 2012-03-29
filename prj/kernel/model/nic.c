#include <nic.h>
#include <spinlock.h>
#include <irq.h>
#include <string.h>
#include <mbox.h>
#include <lib/marshal.h>

#include <lwip/opt.h>
#include <lwip/def.h>
#include <lwip/mem.h>
#include <lwip/pbuf.h>
#include <lwip/sys.h>
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include <netif/etharp.h>
#include <netif/ppp_oe.h>
#include <lwip/tcpip.h>
#include <lwip/netifapi.h>
#include <lwip/ip.h>

static list_entry_s nic_free_list;
nic_s nics[NICS_MAX_COUNT];
spinlock_s nic_alloc_lock;

static char nic_ctl_proc_stack[10240];
static proc_s nic_ctl_p;
static spinlock_s nic_ctl_list_lock;
static list_entry_s nic_ctl_list;
static semaphore_s nic_ctl_sem;

static void
nic_ctl_ack(mbox_io_t io, void *__data, uintptr_t hint_a, uintptr_t hint_b)
{
	struct nic_ctl_s *ctl = (struct nic_ctl_s *)__data;
	
	if (io)
	{
		if (hint_a > NIC_CTL_BUF_SIZE)
			hint_a = NIC_CTL_BUF_SIZE;

		memmove(ctl->buf, io->iobuf, hint_a);
	}
	ctl->func = hint_b;
	
	int irq = irq_save();
	spinlock_acquire(&nic_ctl_list_lock);
	list_add(&nic_ctl_list, &ctl->ctl_list);
	spinlock_release(&nic_ctl_list_lock);
	irq_restore(irq);

	semaphore_release(&nic_ctl_sem);
}

static void nic_ctl_proc(void *__ignore);

int
nic_init(void)
{
	list_init(&nic_free_list);
	list_init(&nic_ctl_list);
	spinlock_init(&nic_ctl_list_lock);
	spinlock_init(&nic_alloc_lock);
	semaphore_init(&nic_ctl_sem, 0);

	int i;
	for (i = 0; i != NICS_MAX_COUNT; ++ i)
	{
		nics[i].status = NIC_STATUS_FREE;
		list_add_before(&nic_free_list, &nics[i].free_list);
	}

	proc_init(&nic_ctl_p, ".nic", SCHED_CLASS_RR, (void(*)(void *))nic_ctl_proc, NULL, (uintptr_t)ARCH_STACKTOP(nic_ctl_proc_stack, 10240));
	proc_notify(&nic_ctl_p);	

	return 0;
}


int
nic_alloc(user_proc_t proc, int *mbox_tx, int *mbox_ctl)
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

		nic->status   = NIC_STATUS_UNINIT;
		nic->proc     = proc;
		nic->mbox_tx  = mbox_alloc(proc);
		nic->mbox_ctl = mbox_alloc(proc);

		*mbox_tx = nic->mbox_tx;
		*mbox_ctl = nic->mbox_ctl;

		/* internal ack for start the CRL loop */
		nic_ctl_ack(NULL, &nic->ctl, 0, NIC_CTL_INIT);
		return nic - nics;
	}
	else return -1;
}

void
nic_close(int nic_id)
{
	/* XXX */
}

#define IFNAME0 'i'
#define IFNAME1 'f'

static void
low_level_init(struct netif *netif)
{
	nic_t nic = netif->state;
	char *ptr = nic->cfg_buf;

	MARSHAL_DECLARE(buf, nic->cfg_buf, nic->cfg_buf + NIC_CFG_BUF_SIZE);
	
	uintptr_t flag;
	UNMARSHAL(buf, sizeof(uintptr_t), &flag);
	UNMARSHAL(buf, sizeof(uintptr_t), &netif->mtu);
	UNMARSHAL(buf, sizeof(uintptr_t), &netif->hwaddr_len);
	UNMARSHAL(buf, netif->hwaddr_len, &netif->hwaddr[0]);

	int addr_len;
	struct ip_addr ip, nm, gw;

	UNMARSHAL(buf, sizeof(uintptr_t), &addr_len);
	if (addr_len > sizeof(struct ip_addr)) addr_len = sizeof(struct ip_addr);
	UNMARSHAL(buf, addr_len, &ip);

	UNMARSHAL(buf, sizeof(uintptr_t), &addr_len);
	if (addr_len > sizeof(struct ip_addr)) addr_len = sizeof(struct ip_addr);
	UNMARSHAL(buf, addr_len, &nm);

	UNMARSHAL(buf, sizeof(uintptr_t), &addr_len);
	if (addr_len > sizeof(struct ip_addr)) addr_len = sizeof(struct ip_addr);
	UNMARSHAL(buf, addr_len, &gw);

	netif_set_addr(netif, &ip, &nm, &gw);

	cprintf("mac = %02x:%02x:%02x:%02x:%02x:%02x\n",
			netif->hwaddr[0], netif->hwaddr[1],
			netif->hwaddr[2], netif->hwaddr[3],
			netif->hwaddr[4], netif->hwaddr[5]);
	cprintf("mtu = %d\n", netif->mtu);

	/* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
}

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
	 // uint32_t old_tick = *hpet_tick;
	 nic_t nic = netif->state;
	 struct pbuf *q;
	 int len;

	 mbox_io_t io = mbox_io_acquire(nic->mbox_tx);
	 char *buf = io->iobuf;
	 
#if ETH_PAD_SIZE
	 pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

	 len = 0;
	 
	 for(q = p; q != NULL; q = q->next) {
		  memmove(buf + len, q->payload, q->len);
		  len += q->len;
	 }

	 mbox_io_send(io, NULL, NULL, 0, len);

#if ETH_PAD_SIZE
	 pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif
  
	 LINK_STATS_INC(link.xmit);
	 return ERR_OK;
}

void
nic_input(int nic_id, void *packet, size_t packet_size)
{
	 struct eth_hdr *ethhdr;
	 struct netif *netif = &nics[nic_id].netif;
	 unsigned cur = 0;
	 struct pbuf *p, *q;

#if ETH_PAD_SIZE
	 packet_size += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

	 /* We allocate a pbuf chain of pbufs from the pool. */
	 p = pbuf_alloc(PBUF_RAW, packet_size, PBUF_POOL);
  
	 if (p != NULL) {

#if ETH_PAD_SIZE
		  pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif
		  for(q = p; q != NULL; q = q->next) {
			   memmove(q->payload, packet + cur, q->len);
			   cur += q->len;
		  }
#if ETH_PAD_SIZE
		  pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

		  LINK_STATS_INC(link.recv);
	 } else {
		  LINK_STATS_INC(link.memerr);
		  LINK_STATS_INC(link.drop);

		  return;
	 }

	 /* points to packet payload, which starts with an Ethernet header */
	 ethhdr = p->payload;
	 err_t err;
	 switch (htons(ethhdr->type)) {
		  /* IP or ARP packet? */
	 case ETHTYPE_IP:
	 case ETHTYPE_ARP:
#if PPPOE_SUPPORT
		  /* PPPoE packet? */
	 case ETHTYPE_PPPOEDISC:
	 case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
		  /* full packet send to tcpip_thread to process */
		  // ekf_kprintf("%d %d ::recv to tcpip %d\n", timer_tick_get(), proc_current(), packet_size);
		  err = netif->input(p, netif);
		  if (err != ERR_OK)
		  {
			   LWIP_DEBUGF(NETIF_DEBUG, ("ld_net_if_input: IP input error\n"));
			   pbuf_free(p);
			   p = NULL;
		  }
		  break;

	 default:
		  pbuf_free(p);
		  p = NULL;
		  break;
	 }
}

static err_t
my_etharp_output(struct netif *netif, struct pbuf *q, struct ip_addr *ipaddr)
{
	 // ekf_kprintf("%d etharp output\n", timer_tick_get());
	 return etharp_output(netif, q, ipaddr);
}

static err_t
nic_if_init(struct netif *netif)
{
	 LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
	 /* Initialize interface hostname */
	 netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

	 /*
	  * Initialize the snmp variables and counters inside the struct netif.
	  * The last argument should be replaced with your link speed, in units
	  * of bits per second.
	  */
	 NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

	 netif->name[0] = IFNAME0;
	 netif->name[1] = IFNAME1;
	 /* We directly use etharp_output() here to save a function call.
	  * You can instead declare your own function an call etharp_output()
	  * from it if you have to do some checks before sending (e.g. if link
	  * is available...) */
	 netif->output = my_etharp_output;
	 netif->linkoutput = low_level_output;
  
	 /* initialize the hardware */
	 low_level_init(netif);

	 return ERR_OK;
}

static void
nic_ctl_proc(void *__ignore)
{
	while (1)
	{
		semaphore_acquire(&nic_ctl_sem, NULL);

		list_entry_t l;
		int irq = irq_save();
		spinlock_acquire(&nic_ctl_list_lock);
		l = list_next(&nic_ctl_list);
		list_del(l);
		spinlock_release(&nic_ctl_list_lock);
		irq_restore(irq);

		struct nic_ctl_s *ctl = CONTAINER_OF(l, struct nic_ctl_s, ctl_list);
		nic_t nic = CONTAINER_OF(ctl, nic_s, ctl);

		cprintf("NIC CTL %d!\n", ctl->func);
		switch(ctl->func)
		{
		case NIC_CTL_CFG_SET:
		{
			memmove(nic->cfg_buf, ctl->buf, NIC_CFG_BUF_SIZE);
			if (nic->status == NIC_STATUS_UNINIT)
				nic->status = NIC_STATUS_DETACHED;
		}
		break;
		
		case NIC_CTL_ADD:
		{
			static struct ip_addr ip_zero = { 0 };
			if (nic->status == NIC_STATUS_DETACHED)
			{
				netif_add(&nic->netif, &ip_zero, &ip_zero, &ip_zero, nic, nic_if_init, tcpip_input);
				nic->status = NIC_STATUS_ATTACHED;
			}
		}
		break;
		
		}
		
		/* Get next ctl */
		mbox_io_send(mbox_io_acquire(nic->mbox_ctl), nic_ctl_ack, ctl, 0, 0);
	}
}
