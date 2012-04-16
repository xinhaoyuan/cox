#ifndef __USER_IO_H__
#define __USER_IO_H__

#include <types.h>
#include <algo/list.h>

#define IO_ARGS_COUNT 6

typedef unsigned int iobuf_index_t;
#define IOBUF_INDEX_NULL ((iobuf_index_t)-1)

struct io_call_entry_s
{
    unsigned status;
    uintptr_t data[IO_ARGS_COUNT];
};

typedef struct io_call_entry_s  io_call_entry_s;
typedef struct io_call_entry_s *io_call_entry_t;

#define IO_CALL_STATUS_FREE 0
#define IO_CALL_STATUS_USED 1
#define IO_CALL_STATUS_WAIT 2
#define IO_CALL_STATUS_PROC 3
#define IO_CALL_STATUS_FIN  4

/* IO FUNC ID LIST */
/* refer to doc/io.txt */

/* BASE */
#define IO_THREAD_NOTIFY    0x000
#define IO_THREAD_CREATE    0x010
#define IO_EXIT             0x011
#define IO_BRK              0x020
#define IO_PAGE_HOLE_SET    0x021
#define IO_PAGE_HOLE_CLEAR  0x022
#define IO_SLEEP            0x030
#define IO_MBOX_ATTACH      0x040
#define IO_MBOX_DETACH      0x041
#define IO_MBOX_IO          0x042
/* DEBUG */
#define IO_DEBUG_PUTCHAR    0x100
#define IO_DEBUG_GETCHAR    0x101
/* MANAGE NODE */
#define IO_IRQ_LISTEN       0x200
#define IO_MMIO_OPEN        0x201
#define IO_MMIO_CLOSE       0x202
#define IO_PHYS_ALLOC       0x203
#define IO_PHYS_FREE        0x204
#define IO_MBOX_OPEN        0x210
#define IO_MBOX_CLOSE       0x211
#define IO_PROC_CREATE      0x220

#define MBOX_IOBUF_POLICY_DO_MAP     0x1
#define MBOX_IOBUF_POLICY_PERSISTENT 0x2

typedef void (*io_callback_handler_f)(void *ret);

#endif
