#include <cpu.h>
#include <string.h>
#include <entry.h>
#include <user.h>
#include <page.h>
#include <malloc.h>
#include <ips.h>
#include <proc.h>
#include <lib/low_io.h>
#include <lwip/sockets.h>
#include <lwip.h>

#define TEST_USER  0
#define TEST_MUTEX 1
#define TEST_TIMER 2
#define TEST_LWIP  3

#define TEST TEST_LWIP

proc_s p1, p2;
mutex_s l;

static void test_2(void);

static void
test_1(void)
{
#if TEST == TEST_TIMER
	cprintf("sleep\n");
	proc_delayed_self_notify_set(100);
	proc_wait_try();
	cprintf("wakeup\n");
#elif TEST == TEST_MUTEX
	while (1)
	{
		mutex_acquire(&l, NULL);
		cprintf("FROM test_1\n");
		mutex_release(&l);
	}
#elif TEST == TEST_LWIP
	int s = lwip_socket(AF_INET, SOCK_STREAM, 0);

	if (s < 0)
	{
		cprintf("SOCKET failed\n");
		return;
	}
	
	struct sockaddr_in sin;
	sin.sin_len = sizeof(sin);
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(7);
	memset(&sin.sin_zero, 0, sizeof(sin.sin_zero));
	
	if (lwip_bind(s, (struct sockaddr *)&sin, sizeof(sin)))
	{
		cprintf("BIND failed\n");
		return;
	}
	
	if (lwip_listen(s, 10))
	{
		cprintf("LISTEN failed\n");
		return;
	}

	void *stack = VADDR_DIRECT(page_alloc_atomic(4));
	proc_init(&p2, ".test-2", SCHED_CLASS_RR, (void(*)(void *))test_2, NULL, (uintptr_t)ARCH_STACKTOP(stack, 4 * PGSIZE));
	proc_notify(&p2);
	
	cprintf("listening....\n");
	struct sockaddr_in clia;
	u32_t clias;
	while (1)
	{
		clias = sizeof(clia);
		int cli = lwip_accept(s, (struct sockaddr *)&clia, &clias);
		if (cli == -1)
		{
			break;
		}
		cprintf("accepted new client\n");
#define SIZE (1024 * 400)
		char *c = (char *)kmalloc(SIZE);
		int r;
		// cprintf("!! %d to recv\n", timer_tick_get());
		while ((r = lwip_recv(cli, c, SIZE, 0)) > 0)
		{
			// cprintf("!! %d %d\n", timer_tick_get(), r);
			// cprintf("!! %d to send\n", timer_tick_get());
			lwip_send(cli, c, r, 0);
			// cprintf("!! %d to recv\n", timer_tick_get());
		}
		kfree(c);
		lwip_close(cli);
		cprintf("to accept\n");
	}
	
	cprintf("echo server exit\n");
	
#endif
}

static void
test_2(void)
{
#if TEST == TEST_MUTEX
	while (1)
	{
		mutex_acquire(&l, NULL);
		cprintf("FROM test_2\n");
		mutex_release(&l);
	}
#elif TEST == TEST_LWIP

	int s = lwip_socket(AF_INET, SOCK_STREAM, 0);

	if (s < 0)
	{
		cprintf("SOCKET failed\n");
		return;
	}
	
	struct sockaddr_in sin;
	sin.sin_len = sizeof(sin);
	sin.sin_addr.s_addr = PP_HTONL(IPADDR_LOOPBACK);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(7);
	memset(&sin.sin_zero, 0, sizeof(sin.sin_zero));

	int r = connect(s, (struct sockaddr *)&sin, sizeof(sin));
	cprintf("connect: %d\n", r);
	if (r >= 0)
	{
		lwip_close(s);
	}
	
#endif
}

void
kernel_start(void)
{
	cprintf("KERNEL START\n");

#if TEST == TEST_USER

	extern char _binary_user_init_image_start[];
	extern char _binary_user_init_image_end[];
	int ret = user_proc_load((void *)_binary_user_init_image_start,
							 _binary_user_init_image_end - _binary_user_init_image_start);
	cprintf("Jump to user\n");
	user_thread_jump();

#elif TEST == TEST_MUTEX

	mutex_init(&l);

	void *stack;

	stack = VADDR_DIRECT(page_alloc_atomic(4));
	proc_init(&p1, ".test-1", SCHED_CLASS_RR, (void(*)(void *))test_1, NULL, (uintptr_t)ARCH_STACKTOP(stack, 4 * PGSIZE));
	proc_notify(&p1);
	
	stack = VADDR_DIRECT(page_alloc_atomic(4));
	proc_init(&p2, ".test-2", SCHED_CLASS_RR, (void(*)(void *))test_2, NULL, (uintptr_t)ARCH_STACKTOP(stack, 4 * PGSIZE));
	proc_notify(&p2);

#elif TEST == TEST_TIMER

	void *stack;

	stack = VADDR_DIRECT(page_alloc_atomic(4));
	proc_init(&p1, ".test-1", SCHED_CLASS_RR, (void(*)(void *))test_1, NULL, (uintptr_t)ARCH_STACKTOP(stack, 4 * PGSIZE));
	proc_notify(&p1);

#elif TEST == TEST_LWIP
	
	kern_lwip_init();

	void *stack;

	stack = VADDR_DIRECT(page_alloc_atomic(4));
	proc_init(&p1, ".test-1", SCHED_CLASS_RR, (void(*)(void *))test_1, NULL, (uintptr_t)ARCH_STACKTOP(stack, 4 * PGSIZE));
	proc_notify(&p1);
	
#endif
	
	cprintf("KERNEL OVER\n");
	while (1) __cpu_relax();
}
