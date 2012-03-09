#ifndef __IO_H__
#define __IO_H__

#include <cpu.h>
#include <types.h>

#define IO_ARGS_COUNT 8

typedef unsigned int iobuf_index_t;

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
			iobuf_index_t head, tail;
		} head;
	};
};
typedef struct io_call_entry_s  io_call_entry_s;
typedef struct io_call_entry_s *io_call_entry_t;

#define IO_CALL_STATUS_FREE 0
#define IO_CALL_STATUS_USED 1
#define IO_CALL_STATUS_WAIT 2
#define IO_CALL_STATUS_PROC 3
#define IO_CALL_STATUS_FIN  4
/* BASE */
#define IO_SET_CALLBACK     0x000
#define IO_SET_BUF          0x001
/* DEBUG */
#define IO_DEBUG_PUTCHAR    0x101
#define IO_DEBUG_GETCHAR    0x102
/* STD SOCKET API */
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
/* OWNERSHIP TRANSFER */
#define IO_SOCK_TRANSFER    0x210
/* NAME RESOLVING */
#define IO_GET_HOST_BY_NAME 0x220
#define IO_GET_ADDR_INFO    0x221
#define IO_FREE_ADDR_INFO   0x222
/* SERVICE */
#define IO_SERVICE_ATTACH   0x300
/* NIC NODE */
#define IO_NIC_OPEN         0x400
#define IO_NIC_CLAIM        0x401
#define IO_NIC_CLOSE        0x402
#define IO_NIC_GET_IN_REQ   0x403
#define IO_NIC_GET_OUT_REQ  0x404

typedef void (*io_callback_handler_f)(void *ret);

#endif
