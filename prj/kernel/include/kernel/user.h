#ifndef __KERN_USER_H__
#define __KERN_USER_H__

#include <types.h>
#include <proc.h>
#include <timer.h>
#include <user/io.h>
#include <arch/user.h>
#include <spinlock.h>
#include <mbox.h>

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
#define IO_CE_SHADOW_TYPE_TIMER        2

typedef struct io_ce_shadow_s io_ce_shadow_s;
typedef io_ce_shadow_s *io_ce_shadow_t;

struct user_thread_s
{
    void     *tls;
    uintptr_t tls_u;
    
    uintptr_t iobuf_size;
    uintptr_t iocb_stack_size;
    uintptr_t user_size;
    int       mbox_ex;
    
    struct
    {
        iobuf_index_t   cap;
        io_call_entry_t head;
        io_ce_shadow_t  shadows;
    } ioce;
    
    struct
    {
        spinlock_s     push_lock;
        iobuf_index_t  cap;
        iobuf_index_t *entry;
        uintptr_t     *head, *tail, *busy;
        void          *stack;
        uintptr_t      stack_u;
        uintptr_t      callback_u;
    } iocb;

    user_thread_arch_s arch;
};

typedef struct user_thread_s  user_thread_s;
typedef struct user_thread_s *user_thread_t;

void user_thread_jump(void) __attribute__((noreturn));
int  user_thread_iocb_push(proc_t proc, iobuf_index_t index);

/* filled by arch */
int user_thread_arch_iocb_call(void);
int user_thread_arch_init(proc_t proc, uintptr_t entry);
int user_thread_arch_in_cb_stack(void);
void user_thread_arch_jump(void) __attribute__((noreturn));

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

void user_process_io(proc_t proc);
void user_before_return(proc_t proc);
void user_save_context(proc_t proc);
void user_restore_context(proc_t proc);

void user_arch_save_context(proc_t proc);
void user_arch_restore_context(proc_t proc);

int user_proc_load(void *bin, size_t bin_size);

#endif
