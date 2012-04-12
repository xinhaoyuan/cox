#include <cpu.h>
#include <string.h>
#include <error.h>
#include <proc.h>
#include <user.h>
#include <page.h>
#include <arch/user.h>
#include <user/tls.h>

#include "intr.h"
#include "mem.h"

int
user_thread_arch_init(proc_t proc, uintptr_t entry)
{
    user_thread_t thread = &USER_THREAD(proc);
    
    memset(&thread->arch, 0, sizeof(thread->arch));
    thread->arch.init_entry = entry;
    
    if (thread->data_size +
        thread->iobuf_size != PGSIZE * 2)
    {
        cprintf("PANIC: tls size != PGSIZE * 2");
        return -E_INVAL;
    }

    /* XXX to cleanup */
    /* map tls to kernel */
    thread->tls = valloc(2);
    int i;
    for (i = 0; i < 2; ++ i)
    {
        *get_pte(pgdir_scratch, (uintptr_t)thread->tls + (i << PGSHIFT), 1) =
            *get_pte(thread->user_proc->arch.pgdir, thread->tls_u + (i << PGSHIFT), 0);
    }

    /* refer to user/tls.h */
    thread->iocr_ctl.khead = thread->tls + OFFSET_OF(tls_s, iocr_ctl.khead);
    thread->iocr_ctl.utail = thread->tls + OFFSET_OF(tls_s, iocr_ctl.utail);
    thread->iocb_ctl.uhead = thread->tls + OFFSET_OF(tls_s, iocb_ctl.uhead);
    thread->iocb_ctl.ktail = thread->tls + OFFSET_OF(tls_s, iocb_ctl.ktail);
    
    thread->ioce = thread->tls + thread->data_size;
    thread->iocr = thread->tls + thread->data_size + thread->iobuf_size
        - thread->io_cap * sizeof(iobuf_index_t) * 2;
    thread->iocb = thread->tls + thread->data_size + thread->iobuf_size
        - thread->io_cap * sizeof(iobuf_index_t);

    /* flush the page map */
    __lcr3(__rcr3());
    
    tls_s tls;
    /* XXX: fill the proc arg and thread arg */
    tls.proc_arg        = 0;
    tls.thread_arg      = 0;
    tls.info.io_cap     = thread->io_cap;
    tls.info.ioce       = (void *)(thread->tls_u + ((char *)thread->ioce - (char *)thread->tls));
    tls.info.iocr       = (void *)(thread->tls_u + ((char *)thread->iocr - (char *)thread->tls));
    tls.info.iocb       = (void *)(thread->tls_u + ((char *)thread->iocb - (char *)thread->tls));
    tls.iocr_ctl.khead  = 0;
    tls.iocr_ctl.utail  = 0;
    tls.iocb_ctl.uhead  = 0;
    tls.iocb_ctl.ktail  = 0;
    
    user_proc_arch_copy(thread->user_proc, thread->tls_u, &tls, sizeof(tls));
    return 0;
}

void __user_jump(struct trapframe *tf) __attribute__((noreturn));

void
user_thread_arch_jump(void)
{
    proc_t proc = current;
    if (USER_THREAD(proc).arch.tf != NULL)
    {
        __user_jump(USER_THREAD(proc).arch.tf);
    }
    else
    {
        struct trapframe tf;
        memset(&tf, 0, sizeof(tf));
        tf.tf_cs = GD_UTEXT | 3;
        tf.tf_ss = GD_UDATA | 3;
        tf.tf_ds = GD_UDATA | 3;
        tf.tf_es = GD_UDATA | 3;
        tf.tf_rflags = FL_IF ;
        /* XXX: for test driver node */
        tf.tf_rflags |= FL_IOPL_3;
        tf.tf_rip = USER_THREAD(proc).arch.init_entry;
        /* pass tls, start, end to user init */
        tf.tf_regs.reg_rdi = USER_THREAD(proc).tls_u;
        tf.tf_regs.reg_rsi = USER_THREAD(proc).user_proc->start;
        tf.tf_regs.reg_rdx = USER_THREAD(proc).user_proc->end;
        __user_jump(&tf);
    }
}

void
user_thread_arch_save_context(proc_t proc)
{ }

void
user_thread_arch_restore_context(proc_t proc)
{
    /* change the gs base for TLS */
    __write_msr(0xC0000101, USER_THREAD(proc).tls_u);
    __lcr3(USER_THREAD(proc).user_proc->arch.cr3);
}
