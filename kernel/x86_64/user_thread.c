#include <asm/cpu.h>
#include <string.h>
#include <error.h>
#include <proc.h>
#include <user.h>
#include <frame.h>
#include <arch/user.h>
#include <tls.h>
#include <arch/memlayout.h>

#include "arch/cpu.h"
#include "arch/intr.h"
#include "arch/mem.h"

int
user_thread_arch_state_init(user_thread_t thread, uintptr_t entry, uintptr_t tls, uintptr_t stack)
{
    memset(&thread->arch, 0, sizeof(thread->arch));
    
    thread->arch.init_entry     = entry;
    thread->arch.init_stack_ptr = stack;
    thread->arch.tls            = tls;

    return 0;
}

void
user_thread_arch_destroy(user_thread_t user_thread)
{
    /* XXX */
}

void __user_jump(struct trapframe *tf) __attribute__((noreturn));

void
user_thread_arch_jump(void)
{
    proc_t proc = current;
    user_thread_t thread = USER_THREAD(proc);
    
    if (thread->arch.tf != NULL)
    {
        __user_jump(thread->arch.tf);
    }
    else
    {
        struct trapframe tf;
        memset(&tf, 0, sizeof(tf));
        tf.tf_cs = GD_UTEXT | 3;
        tf.tf_ss = GD_UDATA | 3;
        tf.tf_ds = GD_UDATA | 3;
        tf.tf_es = GD_UDATA | 3;
        tf.tf_rflags = FL_IF;

        /* XXX: IOPL = 3 for test driver node */
        tf.tf_rflags |= FL_IOPL_3;
        tf.tf_rip = thread->arch.init_entry;
        /* pass tls, start, end to user init */
        tf.tf_regs.reg_rdi = thread->arch.tls;
        tf.tf_regs.reg_rsi = thread->user_proc->start;
        tf.tf_regs.reg_rdx = thread->user_proc->end;
        tf.tf_rsp = thread->arch.init_stack_ptr;
        
        __user_jump(&tf);
    }
}

void
user_thread_arch_save_context(proc_t proc, int hint)
{
    if (hint & USER_THREAD_CONTEXT_HINT_URGENT)
    {
    }

    if (hint & USER_THREAD_CONTEXT_HINT_LAZY)
    {
    }
}

void
user_thread_arch_restore_context(proc_t proc)
{
    /* set the U->K stack */
    cpu_set_trap_stacktop((uintptr_t)ARCH_STACKTOP(USER_THREAD(proc)->stack_base, (uintptr_t)USER_THREAD(proc)->stack_size));
    
    /* change the gs base for TLS */
    __write_msr(0xC0000101, USER_THREAD(proc)->arch.tls);
    __lcr3(USER_THREAD(proc)->user_proc->arch.cr3);
}
