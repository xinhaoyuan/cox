#ifndef __IO_H__
#define __IO_H__

#include <runtime/local.h>

/* used for free call entry */
#define IO_ARG_FREE_PREV 0
#define IO_ARG_FREE_NEXT 1

#define IO_DATA_PTR(iod)           ((void *)((iod)->data & ~(uintptr_t)3))
#define IO_DATA_PTR_SET(iod, ptr)  ((iod)->data = ((iod)->data & 3) | ((uintptr_t)(ptr) & ~(uintptr_t)3))
#define IO_DATA_WAIT(iod)          ((iod)->data & 1)
#define IO_DATA_WAIT_SET(iod)      do { (iod)->data |= 1; } while (0)
#define IO_DATA_WAIT_CLEAR(iod)    do { (iod)->data &= ~(uintptr_t)1; } while (0)

struct io_data_s
{
    struct
    {
        union
        {
            short int argc;
            short int index;
        };
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

uintptr_t get_tick(void);
#define TICK get_tick()

io_call_entry_t mbox_io_begin(io_data_t iod);
void *mbox_io_attach(io_data_t iod, int mbox, int to_send, size_t buf_size);
void  mbox_io_end(io_data_t iod);
void  mbox_do_io(io_data_t iod, int mode);

#endif
