#ifndef __KERN_LWIP_LWIPOPTS_H__
#define __KERN_LWIP_LWIPOPTS_H__

#define NO_SYS 0
#define LWIP_COMPAT_MUTEX 1
#define LWIP_NETIF_API 1
#define LWIP_HAVE_LOOPIF 1
#define LWIP_NETIF_LOOPBACK_MULTITHREADING 1
#define TCPIP_MBOX_SIZE 4096

#define DEFAULT_RAW_RECVMBOX_SIZE   4096
#define DEFAULT_TCP_RECVMBOX_SIZE   4096
#define DEFAULT_UDP_RECVMBOX_SIZE   4096

/* CHECKSUM OFFLOAD */
/* #define CHECKSUM_GEN_IP    0 */
/* #define CHECKSUM_GEN_UDP   0 */
/* #define CHECKSUM_GEN_TCP   0 */
/* #define CHECKSUM_CHECK_IP  0 */
/* #define CHECKSUM_CHECK_TCP 0 */
/* #define CHECKSUM_CHECK_UDP 0 */

#define LWIP_RAW      1
#define ETH_PAD_SIZE  2
#define MEM_ALIGNMENT 4
// #define MEM_USE_POOLS 1
// #define PBUF_LINK_HLEN  14

#define TCP_MSS                 1440
#define TCP_SND_BUF             (16 * TCP_MSS)
#define TCP_WND                 (40 * TCP_MSS)
#define TCP_SND_QUEUELEN        (16 * TCP_SND_BUF/TCP_MSS)

#define MEM_SIZE                (4096 * 256)
#define MEMP_NUM_PBUF           10240
#define PBUF_POOL_SIZE          4096
#define MEMP_NUM_TCPIP_MSG_INPKT 1024
#define MEMP_NUM_TCP_SEG        (4 * TCP_SND_QUEUELEN)
#define MEMP_NUM_SYS_TIMEOUT    256
#define MEMP_NUM_REASSDATA      64
#define MEMP_NUM_ARP_QUEUE      256
#define IP_REASS_MAX_PBUFS      (2 * MEMP_NUM_REASSDATA)

#define LWIP_NOASSERT        1
#define SYS_LIGHTWEIGHT_PROT 1

#define LWIP_DEBUG         0
/* #define NETIF_DEBUG        LWIP_DBG_ON */
/* #define MEM_DEBUG          LWIP_DBG_ON */
/* #define MEMP_DEBUG         LWIP_DBG_ON */
/* #define SYS_DEBUG          LWIP_DBG_ON */
/* #define SOCKETS_DEBUG      LWIP_DBG_ON */
/* #define TCP_DEBUG          LWIP_DBG_ON */
/* #define TCP_INPUT_DEBUG    LWIP_DBG_ON */
/* #define TCP_OUTPUT_DEBUG   LWIP_DBG_ON */
/* #define TCPIP_DEBUG        LWIP_DBG_ON */
/* #define ETHARP_DEBUG       LWIP_DBG_ON */
/* #define TCP_WND_DEBUG      LWIP_DBG_ON */
/* #define TCP_CWND_DEBUG     LWIP_DBG_ON */
/* #define LWIP_DBG_TYPES_ON  0xfffff */
/* #define LWIP_DBG_MIN_LEVEL 0 */

#endif
