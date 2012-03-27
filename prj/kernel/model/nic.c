#include <nic.h>
#include <spinlock.h>
#include <irq.h>
#include <string.h>
#include <mbox.h>

static list_entry_s nic_free_list;
nic_s nics[NICS_MAX_COUNT];
spinlock_s nic_alloc_lock;

int
nic_init(void)
{
	list_init(&nic_free_list);

	spinlock_init(&nic_alloc_lock);

	int i;
	for (i = 0; i != NICS_MAX_COUNT; ++ i)
	{
		nics[i].status = NIC_STATUS_FREE;
		list_add_before(&nic_free_list, &nics[i].free_list);
	}

	return 0;
}


int
nic_alloc(user_proc_t proc, int mbox_w)
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

		nic->status = NIC_STATUS_UNINIT;
		nic->proc   = proc;
		nic->mbox_w = mbox_w;
		nic->netif  = NULL;

		return nic - nics;
	}
	else return -1;
}

void
nic_close(int nic_id)
{
	/* XXX */
}

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

#define IFNAME0 'i'
#define IFNAME1 'f'

static void
low_level_init(struct netif *netif)
{
  /* struct ld_net_if *ld_net_if = netif->state; */
  /* fill MAC address */
  /* unsigned int mac_len; */
  /* __LD_if_get_mac(ld_net_if->index, &netif->hwaddr[0], NETIF_MAX_HWADDR_LEN, &mac_len); */
  /* netif->hwaddr_len = mac_len; */
  /* netif->mtu = __LD_if_get_mtu(ld_net_if->index); */

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

	 mbox_io_t io = mbox_io_acquire(nic->mbox_w);
	 char *buf = io->ubuf;
	 
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
	 struct netif *netif = nics[nic_id].netif;
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
	 nic_t nic = netif->state;

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

/* struct netif * */
/* ld_net_ifs_init(void) */
/* { */
/* 	 struct ip_addr ipaddr, netmask, gw; */
/* 	 IP4_ADDR(&gw, 192,168,56,1); */
/* 	 IP4_ADDR(&ipaddr, 192,168,56,2); */
/* 	 IP4_ADDR(&netmask, 255,255,255,0); */
	 
/* 	 struct netif *first = NULL; */
	 
/* 	 ekf_kprintf("if count in linux_drivers: %d\n", __LD_ifcnt); */
/* 	 int i; */
/* 	 for (i = 0; i != LD_NET_IF_MAXCOUNT; ++ i) */
/* 	 { */
/* 		  if (!__LD_if_actived[i]) continue; */
/* 		  ekf_kprintf("manage linux if %d\n", i); */
/* 		  ld_net_if_array[i].index = i; */
/* 		  ld_net_if_array[i].netif = &ld_if_array[i]; */
/* 		  if (first == NULL) */
/* 			   first = &ld_if_array[i]; */
/* 		  netif_add(&ld_if_array[i], &ipaddr, &netmask, &gw, &ld_net_if_array[i], ld_net_if_init, tcpip_input); */
/* 	 } */
/* 	 __LD_if_register_rx_handler(ld_net_if_input_handler); */
/* 	 return first; */
/* } */
