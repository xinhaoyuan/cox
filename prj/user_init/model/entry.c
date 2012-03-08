#include <string.h>
#include <user/io.h>
#include <user/info.h>
#include <user/syscall.h>
#include <user/iocb.h>

struct startup_info_s info;

void
__iocb(void *ret)
{
	*info.iocb.head = *info.iocb.tail;
	*info.iocb.busy = 0;
	iocb_return(ret);
}

void
entry(struct startup_info_s __info)
{
	memmove(&info, &__info, sizeof(info));
			
	info.ioce.head[1].ce.data[0] = IO_SET_CALLBACK;
	info.ioce.head[1].ce.data[1] = (uintptr_t)__iocb;
	info.ioce.head[1].ce.next = 2;

	info.ioce.head->head.tail = 2;
	
	info.ioce.head->head.head = 1;
	
	__io();

	info.ioce.head[2].ce.data[0] = IO_DEBUG_PUTCHAR;
	info.ioce.head[2].ce.data[1] = (uintptr_t)':';
	info.ioce.head[2].ce.next = 3;
	info.ioce.head->head.tail = 3;	

	__io();

	info.ioce.head[3].ce.data[0] = IO_DEBUG_PUTCHAR;
	info.ioce.head[3].ce.data[1] = (uintptr_t)')';
	info.ioce.head[3].ce.next = 4;
	info.ioce.head->head.tail = 4;

	__io();

	
	while (1) ;
}
