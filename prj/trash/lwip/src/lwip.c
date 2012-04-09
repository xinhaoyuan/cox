#include <lwip/stats.h>
#include <lwip/sys.h>
#include <lwip/mem.h>
#include <lwip/memp.h>
#include <lwip/pbuf.h>
#include <netif/etharp.h>
#include <lwip/udp.h>
#include <lwip/tcp.h>
#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/init.h>
#include <lwip.h>

void
kern_lwip_init(void)
{
	sys_sem_t sem;

	sys_sem_new(&sem, 0);
	tcpip_init((void(*)(void *))sys_sem_signal, &sem);
	sys_sem_wait(&sem);
	sys_sem_free(&sem);
}

