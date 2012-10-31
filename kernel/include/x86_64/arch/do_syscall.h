#ifndef __KERN_ARCH_DO_SYSCALL_H__
#define __KERN_ARCH_DO_SYSCALL_H__

#include "trapframe.h"

typedef struct trapframe *syscall_context_t;

#define SYSCALL_ARG0_GET(CTX) ((CTX)->tf_regs.reg_rax)
#define SYSCALL_ARG1_GET(CTX) ((CTX)->tf_regs.reg_rbx)
#define SYSCALL_ARG2_GET(CTX) ((CTX)->tf_regs.reg_rdx)
#define SYSCALL_ARG3_GET(CTX) ((CTX)->tf_regs.reg_rcx)
#define SYSCALL_ARG4_GET(CTX) ((CTX)->tf_regs.reg_rdi)
#define SYSCALL_ARG5_GET(CTX) ((CTX)->tf_regs.reg_rsi)

#define SYSCALL_ARG0_SET(CTX,VALUE) do { (CTX)->tf_regs.reg_rax = (VALUE); } while (0)
#define SYSCALL_ARG1_SET(CTX,VALUE) do { (CTX)->tf_regs.reg_rbx = (VALUE); } while (0)
#define SYSCALL_ARG2_SET(CTX,VALUE) do { (CTX)->tf_regs.reg_rdx = (VALUE); } while (0)
#define SYSCALL_ARG3_SET(CTX,VALUE) do { (CTX)->tf_regs.reg_rcx = (VALUE); } while (0)
#define SYSCALL_ARG4_SET(CTX,VALUE) do { (CTX)->tf_regs.reg_rdi = (VALUE); } while (0)
#define SYSCALL_ARG5_SET(CTX,VALUE) do { (CTX)->tf_regs.reg_rsi = (VALUE); } while (0)

#endif
