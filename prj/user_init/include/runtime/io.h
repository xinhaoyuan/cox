#ifndef __IO_H__
#define __IO_H__

#include <runtime/local.h>

void __iocb(void *ret);

inline static int __io_save(void) __attribute__((always_inline));
inline static void __io_restore(int status) __attribute__((always_inline));

inline static int
__io_save(void)
{ int r = __io_ubusy; __io_ubusy_set(1); return r; }

inline static void
__io_restore(int status)
{
	if (status == 0)
	{
		__io_ubusy_set(0);
		if (__iocb_busy)
			__iocb(NULL);
	}
	else __io_ubusy_set(1);
}

#define IO_DATA_PTR(iod)           ((void *)((iod)->data & ~(uintptr_t)3))
#define IO_DATA_PTR_SET(iod, ptr)  ((iod)->data = ((iod)->data & 3) | ((uintptr_t)(ptr) & ~(uintptr_t)3))
#define IO_DATA_WAIT(iod)          ((iod)->data & 1)
#define IO_DATA_WAIT_SET(iod)      do { (iod)->data |= 1; } while (0)
#define IO_DATA_WAIT_CLEAR(iod)    do { (iod)->data &= ~(uintptr_t)1; } while (0)

struct io_data_s
{
	struct
	{
		short int argc;
		short int retc;
	} __attribute__((packed));

	uintptr_t data;
	uintptr_t io[IO_ARGS_COUNT];
};

typedef struct io_data_s io_data_s;
typedef io_data_s *io_data_t;

#define IO_MODE_NO_RET 0
#define IO_MODE_SYNC   1
#define IO_MODE_ASYNC  2

#define IO_DATA_INITIALIZER(_retc_, args ...) { .retc = (_retc_), .argc = sizeof((char[]){ args }), .io = { args } }

void io_init(void);
int  io(io_data_t iod, int mode);

#endif
