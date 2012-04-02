#ifndef __KERN_MBOX_H__
#define __KERN_MBOX_H__

#include <ips.h>
#include <algo/list.h>
#include <user/io.h>
#include <proc.h>

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

        struct nic_s *nic_tx;
        struct nic_s *nic_ctl;
    };

    spinlock_s          lock;
    int                 status;
    struct user_proc_s *proc;
    
    spinlock_s   io_lock;
    list_entry_s recv_io_list;
    list_entry_s send_io_list;
};

typedef struct mbox_s mbox_s;
typedef mbox_s *mbox_t;

struct mbox_io_s;

typedef void(*mbox_ack_callback_f) (void *data, void *buf, uintptr_t hint_a, uintptr_t hint_b);
typedef void(*mbox_send_callback_f)(void *data, void *buf, uintptr_t *hint_a, uintptr_t *hint_b);

struct mbox_send_io_s
{
    union
    {
        list_entry_s  io_list;
        list_entry_s  free_list;
    };

    short  type;
    short  status;
    mbox_t mbox;

    mbox_send_callback_f send_cb;
    void                *send_data;

    mbox_ack_callback_f  ack_cb;
    void                *ack_data;
};

struct mbox_recv_io_s
{
    union
    {
        list_entry_s io_list;
        list_entry_s free_list;
    };

    struct mbox_send_io_s *send_io;
    mbox_t  mbox;

    void     *iobuf;
    uintptr_t iobuf_u;
};

#define MBOX_IOBUF_PSIZE 1

#define MBOX_STATUS_FREE       0
#define MBOX_STATUS_NORMAL     1
#define MBOX_STATUS_IRQ_LISTEN 2
#define MBOX_STATUS_NIC_TX     3
#define MBOX_STATUS_NIC_CTL    4

#define MBOX_SEND_IO_TYPE_FREE 0
#define MBOX_SEND_IO_TYPE_KERN 1
#define MBOX_SEND_IO_TYPE_USER 2

#define MBOX_SEND_IO_STATUS_INIT       0
#define MBOX_SEND_IO_STATUS_QUEUEING   1
#define MBOX_SEND_IO_STATUS_PROCESSING 2
#define MBOX_SEND_IO_STATUS_FINISHED   3

typedef struct mbox_send_io_s mbox_send_io_s;
typedef mbox_send_io_s *mbox_send_io_t;

typedef struct mbox_recv_io_s mbox_recv_io_s;
typedef mbox_recv_io_s *mbox_recv_io_t;

#define MBOXS_MAX_COUNT 256
extern mbox_s mboxs[MBOXS_MAX_COUNT];

int  mbox_init(void);
int  mbox_alloc(struct user_proc_s *proc);
void mbox_free(int mbox_id);

mbox_t mbox_get(int mbox_id);
void   mbox_put(mbox_t mbox);

mbox_send_io_t mbox_send_io_acquire(mbox_t mbox, int try);
int            mbox_send(mbox_send_io_t io, mbox_send_callback_f send_cb, void *send_data, mbox_ack_callback_f ack_cb, void *ack_data);

int            mbox_io(mbox_t mbox, proc_t io_proc, iobuf_index_t io_index, uintptr_t ack_hint_a, uintptr_t ack_hint_b);

/* simple ack function/data */

struct mbox_io_data_s
{
    ips_node_t ips;
    void      *send_buf;
    size_t     send_buf_size;
    uintptr_t  hint_send;
    void      *recv_buf;
    size_t     recv_buf_size;
    uintptr_t  hint_ack;
};

typedef struct mbox_io_data_s mbox_io_data_s;
typedef mbox_io_data_s *mbox_io_data_t;

void mbox_ack_func(void *data, void *buf, uintptr_t hint_a, uintptr_t hint_b);
void mbox_send_func(void *data, void *buf, uintptr_t *hint_a, uintptr_t *hint_b);

#endif
