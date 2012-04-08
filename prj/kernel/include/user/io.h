#ifndef __USER_IO_H__
#define __USER_IO_H__

#include <types.h>
#include <algo/list.h>

#define IO_ARGS_COUNT 6
#define IO_ARG_UD     (IO_ARGS_COUNT - 1)

typedef unsigned int iobuf_index_t;
#define IOBUF_INDEX_NULL ((iobuf_index_t)-1)

struct io_call_entry_s
{
    union
    {
        struct
        {
            unsigned int status;
            iobuf_index_t prev, next;

            uintptr_t data[IO_ARGS_COUNT];
        } ce;

        struct
        {
            iobuf_index_t head; /* when = 0, could be written by user for a new start */
            iobuf_index_t tail, free; /* by user */
        } ctl;
    };
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
#define IO_SET_CALLBACK     0x000
#define IO_CREATE_PROC      0x010
#define   IO_PROC_CREATE_FLAG_SHARE  0x1
#define   IO_PROC_CREATE_FLAG_MNODE  0x2
#define IO_EXIT             0x011
#define IO_BRK              0x020
#define IO_PAGE_HOLE_SET    0x021
#define IO_PAGE_HOLE_CLEAR  0x022
#define IO_SLEEP            0x030
/* DEBUG */
#define IO_DEBUG_PUTCHAR    0x100
#define IO_DEBUG_GETCHAR    0x101
/* STD SOCKET API */
/* All arguments should be same as standard interface */
#define IO_SOCK_OPEN        0x200
#define IO_SOCK_BIND        0x201
#define IO_SOCK_LISTEN      0x202
#define IO_SOCK_ACCEPT      0x203
#define IO_SOCK_CONNECT     0x204
#define IO_SOCK_SET_OPT     0x205
#define IO_SOCK_GET_OPT     0x206
#define IO_SOCK_IOCTL       0x207
#define IO_SOCK_SELECT      0x208
#define IO_SOCK_RECV        0x209
#define IO_SOCK_RECV_FROM   0x20a
#define IO_SOCK_SEND        0x20b
#define IO_SOCK_SEND_TO     0x20c
#define IO_SOCK_SHUTDOWN    0x20d
#define IO_SOCK_CLOSE       0x20e
/* PAGE OPTIMIZED TRANSFER */
#define IO_SOCK_SEND_PAGE   0x210
#define IO_SOCK_RECV_PAGE   0x211
/* NAME RESOLVING */
#define IO_GET_HOST_BY_NAME 0x220
#define IO_GET_ADDR_INFO    0x221
#define IO_FREE_ADDR_INFO   0x222
/* MANAGE NODE */
#define IO_IRQ_LISTEN       0x400
#define IO_MMIO_OPEN        0x401
#define IO_MMIO_CLOSE       0x402
#define IO_PHYS_ALLOC       0x403
#define IO_PHYS_FREE        0x404
#define IO_MBOX_OPEN        0x410
#define IO_MBOX_CLOSE       0x411
#define IO_MBOX_IO          0x412
#define IO_NIC_OPEN         0x420
#define IO_NIC_CLOSE        0x421

#define NIC_CTL_BUF_SIZE 0x100
#define NIC_CTL_INIT     0x0
#define NIC_CTL_CFG_SET  0x1 
#define NIC_CTL_ADD      0x2
#define NIC_CTL_UP       0x3
#define NIC_CTL_DOWN     0x4

typedef void (*io_callback_handler_f)(void *ret);

#endif
