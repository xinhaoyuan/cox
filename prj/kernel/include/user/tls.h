#ifndef __USER_TLS_H__
#define __USER_TLS_H__

#include <user/io.h>

struct tls_s
{
	uintptr_t iocb_busy;
	uintptr_t iocb_head;
	uintptr_t iocb_tail;

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
	} startup_info;

	uintptr_t data[0];
};

typedef struct tls_s  tls_s;
typedef struct tls_s *tls_t;

#endif
