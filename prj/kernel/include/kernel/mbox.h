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

	union
	{
		struct
		{
			list_entry_s listen_list;
			int          irq_no;
		} irq_listen;
	};

	int status;
	user_proc_t proc;
	
	spinlock_s   io_lock;
	list_entry_s io_recv_list;
	list_entry_s io_send_list;
};

typedef struct mbox_s mbox_s;
typedef mbox_s *mbox_t;

struct mbox_io_s;

typedef void(*mbox_ack_callback_f)(struct mbox_io_s *io, void *data, uintptr_t hint_a, uintptr_t hint_b);
typedef void(*mbox_send_callback_f)(struct mbox_io_s *io, void *data, uintptr_t *hint_a, uintptr_t *hint_b);

struct mbox_io_s
{
	union
	{
		list_entry_s  io_list;
		list_entry_s  free_list;
	};

	int           status;
	mbox_t        mbox;

	union
	{
		struct
		{
			mbox_send_callback_f send_cb;
			void                *send_data;
		};

		struct
		{
			void     *iobuf;
			uintptr_t iobuf_u;
		};
	};
			
	mbox_ack_callback_f  ack_cb;
	void                *ack_data;
};

#define MBOX_IO_IOBUF_PSIZE 1

#define MBOX_STATUS_FREE       0
#define MBOX_STATUS_NORMAL     1
#define MBOX_STATUS_IRQ_LISTEN 2

#define MBOX_IO_STATUS_FREE            0
#define MBOX_IO_STATUS_SEND_QUEUEING   1
#define MBOX_IO_STATUS_RECV_QUEUEING   2
#define MBOX_IO_STATUS_PROCESSING      4
#define MBOX_IO_STATUS_FINISHED        5

typedef struct mbox_io_s mbox_io_s;
typedef mbox_io_s *mbox_io_t;

#define MBOXS_MAX_COUNT     256
#define MBOX_IOS_MAX_COUNT 4096

extern mbox_s mboxs[MBOXS_MAX_COUNT];
extern mbox_io_s mbox_ios[MBOX_IOS_MAX_COUNT];

int  mbox_init(void);
int  mbox_alloc(user_proc_t proc);
void mbox_free(int mbox_id);

int  mbox_send(int mbox_id, int try, mbox_send_callback_f send_cb, void *send_data, mbox_ack_callback_f ack_cb, void *ack_data);
int  mbox_io(int mbox_id, int ack_id, uintptr_t hint_a, uintptr_t hint_b, proc_t io_proc, iobuf_index_t io_index);

/* simple ack function/data */

struct mbox_io_data_s
{
	ips_node_t ips;
	void  *send_buf;
	size_t send_buf_size;
	uintptr_t hint_send;
	void  *recv_buf;
	size_t recv_buf_size;
	uintptr_t hint_ack;
};

typedef struct mbox_io_data_s mbox_io_data_s;
typedef mbox_io_data_s *mbox_io_data_t;

void mbox_send_func(mbox_io_t io, void *data, uintptr_t *hint_a, uintptr_t *hint_b);
void mbox_ack_func(mbox_io_t io, void *data, uintptr_t hint_a, uintptr_t hint_b);

#endif
