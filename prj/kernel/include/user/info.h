#ifndef __USER_INFO_H__
#define __USER_INFO_H__

#include <user/io.h>

struct startup_info_s
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
};

typedef struct startup_info_s  startup_info_s;
typedef struct startup_info_s *startup_info_t;

#endif
