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

struct mbox_send_io_s;
struct io_ce_shadow_s;
struct io_call_entry_s;

typedef void(*mbox_send_callback_f)(struct mbox_send_io_s *send_io, struct io_ce_shadow_s *recv_shd, struct io_call_entry_s *recv_ce);
typedef void(*mbox_ack_callback_f) (struct mbox_send_io_s *send_io, struct io_ce_shadow_s *recv_shd, struct io_call_entry_s *recv_ce);

struct mbox_send_io_s
{
    union
    {
        list_entry_s  io_list;
        list_entry_s  free_list;
    };

    mbox_t    mbox;
    short     type;
    short     status;
    int       iobuf_policy;
    uintptr_t iobuf_target_u;

    union
    {
        struct
        {
            mbox_send_callback_f send_cb;
            mbox_ack_callback_f  ack_cb;
            void                *priv;
        };

        struct
        {
            page_t    iobuf_p;
            size_t    iobuf_size;
            uintptr_t iobuf_u;
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

    int       iobuf_mapped;
    void     *iobuf;
    page_t    iobuf_p;
    uintptr_t iobuf_u;
};

#define MBOX_IOBUF_PSIZE 1

#define MBOX_STATUS_FREE       0
#define MBOX_STATUS_NORMAL     1
#define MBOX_STATUS_IRQ_LISTEN 2

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

int  mbox_sys_init(void);
int  mbox_alloc(struct user_proc_s *proc);
void mbox_free(int mbox_id);

mbox_t mbox_get(int mbox_id);
void   mbox_put(mbox_t mbox);

mbox_send_io_t mbox_send_io_acquire(int try);

int mbox_kern_send(mbox_send_io_t io);
int mbox_user_send(proc_t io_proc, iobuf_index_t io_index);
int mbox_user_recv(proc_t io_proc, iobuf_index_t io_index);
int mbox_user_io_attach(proc_t io_proc, iobuf_index_t io_index, mbox_t mbox, int type, size_t buf_size);
int mbox_user_io_detach(proc_t io_proc, iobuf_index_t io_index);

#endif
