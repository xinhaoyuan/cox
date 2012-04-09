#ifndef __KERN_MBOX_H__
#define __KERN_MBOX_H__

#include <ips.h>
#include <page.h>
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
        struct nic_s *nic_rx;
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

    union
    {
        struct
        {
            mbox_send_callback_f send_cb;
            void                *send_data;
            
            mbox_ack_callback_f  ack_cb;
            void                *ack_data;
        };

        struct
        {
            page_t    iobuf_p;
            uintptr_t iobuf_u;
            uintptr_t hint_a;
            uintptr_t hint_b;
        };
    };
};

struct mbox_recv_io_s
{
    union
    {
        list_entry_s io_list;
        list_entry_s free_list;
    };

    struct mbox_send_io_s *send_io;
    mbox_t    mbox;

    void     *iobuf;
    page_t    iobuf_p;
    uintptr_t iobuf_u;
    uintptr_t iobuf_u_target;
};

#define MBOX_IOBUF_PSIZE 1

#define MBOX_STATUS_FREE       0
#define MBOX_STATUS_NORMAL     1
#define MBOX_STATUS_IRQ_LISTEN 2
#define MBOX_STATUS_NIC_TX     3
#define MBOX_STATUS_NIC_RX     4
#define MBOX_STATUS_NIC_CTL    5

#define MBOX_SEND_IO_TYPE_FREE 0
#define MBOX_SEND_IO_TYPE_KERN_ALLOC  1
#define MBOX_SEND_IO_TYPE_KERN_STATIC 2
#define MBOX_SEND_IO_TYPE_USER_SHADOW 3

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

mbox_send_io_t mbox_send_io_acquire(int try);
int            mbox_kern_send(mbox_send_io_t io);
int            mbox_user_send(mbox_t mbox, proc_t io_proc, iobuf_index_t io_index, uintptr_t hint_a, uintptr_t hint_b);
int            mbox_user_recv(mbox_t mbox, proc_t io_proc, iobuf_index_t io_index, uintptr_t ack_hint_a, uintptr_t ack_hint_b);
int            mbox_user_io_end(proc_t io_proc, iobuf_index_t io_index);

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
