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
#define IO_CALL_STATUS_FIN  3

/* non std io call  */
#define IO_DEBUG 0

#define IO_STD_START 32
#define IO_STD_COUNT 0

#define IO_SET_HANDLER 0
#define IO_SET_BUF     1

typedef void (*io_callback_handler_f)(iobuf_index_t start, iobuf_index_t end);


#endif
