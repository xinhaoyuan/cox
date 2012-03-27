#ifndef __KERN_MBOX_H__
#define __KERN_MBOX_H__

#include <user.h>
#include <ips.h>
#include <algo/list.h>
#include <user/io.h>

struct mbox_s
{
	union
	{
		list_entry_s proc_list;
		list_entry_s free_list;
	};

	int status;
	user_proc_t proc;
	
	semaphore_s  io_sem;
	spinlock_s   io_lock;
	list_entry_s io_list;
};

typedef struct mbox_s mbox_s;
typedef mbox_s *mbox_t;

struct mbox_io_s
{
	union
	{
		list_entry_s  io_list;
		list_entry_s  free_list;
	};

	ips_node_t    ips;
	proc_t        io_proc;
	int           status;
	mbox_t         mbox;
	iobuf_index_t io_index;

	void     *buf;
	size_t    buf_size;
	
	void     *ubuf;
	uintptr_t ubuf_u;
};

#define MBOX_IO_UBUF_PSIZE 1

#define MBOX_STATUS_FREE   0
#define MBOX_STATUS_CLOSED 1
#define MBOX_STATUS_OPEN   2

#define MBOX_IO_STATUS_FREE       0
#define MBOX_IO_STATUS_QUEUEING   1
#define MBOX_IO_STATUS_PROCESSING 2
#define MBOX_IO_STATUS_FINISHED   3

typedef struct mbox_io_s mbox_io_s;
typedef mbox_io_s *mbox_io_t;

#define MBOXS_MAX_COUNT     256
#define MBOX_IOS_MAX_COUNT 4096

extern mbox_s mboxs[MBOXS_MAX_COUNT];
extern mbox_io_s mbox_ios[MBOX_IOS_MAX_COUNT];

int  mbox_init(void);
int  mbox_alloc(user_proc_t proc);
void mbox_free(int mbox_id);

int  mbox_send(int mbox_id, void *buf, size_t data_size, size_t buf_size, uintptr_t hint);
int  mbox_io(int mbox_id, int ack, size_t ack_size, proc_t io_proc, iobuf_index_t io_index);

#endif
