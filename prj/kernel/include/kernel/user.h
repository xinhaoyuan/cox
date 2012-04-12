#ifndef __KERN_USER_H__
#define __KERN_USER_H__

#include <types.h>
#include <proc.h>
#include <timer.h>
#include <user/io.h>
#include <arch/user.h>
#include <spinlock.h>
#include <mbox.h>

struct user_proc_s
{
    /* address range */
    uintptr_t start;
    uintptr_t end;

    /* arch data */
    user_proc_arch_s arch;
};

typedef struct user_proc_s  user_proc_s;
typedef struct user_proc_s *user_proc_t;

int user_proc_copy_page(user_proc_t mm, uintptr_t addr, uintptr_t phys, int flag);
int user_proc_copy(user_proc_t mm, uintptr_t addr, void *src, size_t size);
int user_proc_brk(user_proc_t mm, uintptr_t end);

/* filled by arch */
int user_proc_arch_init(user_proc_t mm, uintptr_t *start, uintptr_t *end);
int user_proc_arch_copy_page(user_proc_t mm, uintptr_t addr, uintptr_t phys, int flag);
int user_proc_arch_copy(user_proc_t mm, uintptr_t addr, void *src, size_t size);
int user_proc_arch_mmio_open(user_proc_t mm, uintptr_t addr, size_t size, uintptr_t *result);
int user_proc_arch_mmio_close(user_proc_t mm, uintptr_t addr);
int user_proc_arch_brk(user_proc_t mm, uintptr_t end);

/* private kernel data associated with each io call entries */
struct io_ce_shadow_s
{
    int            type;
    proc_t         proc;
    iobuf_index_t  index;

    union
    {
        mbox_send_io_s mbox_send_io;
        mbox_recv_io_s mbox_recv_io;
        timer_s        timer;
    };
};

#define IO_CE_SHADOW_TYPE_INIT         0
#define IO_CE_SHADOW_TYPE_MBOX_RECV_IO 1
#define IO_CE_SHADOW_TYPE_MBOX_SEND_IO 2
#define IO_CE_SHADOW_TYPE_TIMER        3

typedef struct io_ce_shadow_s io_ce_shadow_s;
typedef io_ce_shadow_s *io_ce_shadow_t;

struct user_thread_s
{
    user_proc_t  user_proc;
    void        *tls;           /* kernel access of TLS area */
    uintptr_t    tls_u;         /* user access */
    size_t       data_size;     /* size of tls data area */
    size_t       iobuf_size;    /* size for I/O buffer, including ioce, iocr, iocb */

    int mbox_manage;            /* mbox for managing this thread */

    iobuf_index_t   io_cap;     /* io concurrency */
    io_call_entry_t ioce;       /* io call entries */
    io_ce_shadow_t  ioce_shadow; /* kernel priv for ioce */

    iobuf_index_t *iocr;        /* io call requests buffer */

    /* iocr queue ctl */
    struct
    {
        uintptr_t *khead, *utail;
    } iocr_ctl;

    iobuf_index_t *iocb;        /* io call back buffer */

    /* iocb queue ctl */
    struct
    {
        spinlock_s  push_lock;
        uintptr_t  *uhead, *ktail;
    } iocb_ctl;

    user_thread_arch_s arch;    /* arch data */
};

typedef struct user_thread_s  user_thread_s;
typedef struct user_thread_s *user_thread_t;

struct user_thread_wrapper_s
{
    proc_s        proc;
    user_thread_s user_thread;
};

typedef struct user_thread_wrapper_s user_thread_wrapper_s;

#define USER_THREAD(proc) (((user_thread_wrapper_s *)(proc))->user_thread)

void user_thread_jump(void) __attribute__((noreturn));
int  user_thread_iocb_push(proc_t proc, iobuf_index_t index);

/* filled by arch */
int  user_thread_arch_iocb_call(void);
int  user_thread_arch_init(proc_t proc, uintptr_t entry);
int  user_thread_arch_in_cb_stack(void);
void user_thread_arch_jump(void) __attribute__((noreturn));
void user_thread_arch_save_context(proc_t proc);
void user_thread_arch_restore_context(proc_t proc);

void user_thread_before_return(proc_t proc);
void user_thread_save_context(proc_t proc);
void user_thread_restore_context(proc_t proc);
int  user_thread_init_exec(proc_t proc, void *bin, size_t bin_size);

void user_process_io(proc_t proc);

#endif
