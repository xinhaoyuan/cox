#ifndef __USER_TLS_H__
#define __USER_TLS_H__

#include <user/io.h>

struct tls_s
{
	uintptr_t proc_arg;
	uintptr_t thread_arg;
	struct
	{
		uintptr_t busy;
		uintptr_t head;
		uintptr_t tail;
	} iocb;

	struct
	{
		struct
		{
			iobuf_index_t   cap;
			io_call_entry_t head;
		} ioce;
		
		struct
		{
			iobuf_index_t   cap;
			iobuf_index_t  *entry;
		} iocb;
	} info;

	uintptr_t data[0];
};

typedef struct tls_s  tls_s;
typedef struct tls_s *tls_t;

#define TLS_PROC_ARG   0
#define TLS_THREAD_ARG 1
#define TLS_IOCB_BUSY  2
#define TLS_IOCB_HEAD  3
#define TLS_IOCB_TAIL  4
#define TLS_USTART     5

#endif
