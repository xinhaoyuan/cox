#ifndef __KERN_ARCH_DO_SERVICE_H__
#define __KERN_ARCH_DO_SERVICE_H__

#include "trapframe.h"

typedef struct trapframe *service_context_t;

#define SERVICE_ARG0_GET(CTX) ((CTX)->tf_regs.reg_rax)
#define SERVICE_ARG1_GET(CTX) ((CTX)->tf_regs.reg_rbx)
#define SERVICE_ARG2_GET(CTX) ((CTX)->tf_regs.reg_rdx)
#define SERVICE_ARG3_GET(CTX) ((CTX)->tf_regs.reg_rcx)
#define SERVICE_ARG4_GET(CTX) ((CTX)->tf_regs.reg_rdi)
#define SERVICE_ARG5_GET(CTX) ((CTX)->tf_regs.reg_rsi)

#define SERVICE_ARG0_SET(CTX,VALUE) do { (CTX)->tf_regs.reg_rax = (VALUE); } while (0)
#define SERVICE_ARG1_SET(CTX,VALUE) do { (CTX)->tf_regs.reg_rbx = (VALUE); } while (0)
#define SERVICE_ARG2_SET(CTX,VALUE) do { (CTX)->tf_regs.reg_rdx = (VALUE); } while (0)
#define SERVICE_ARG3_SET(CTX,VALUE) do { (CTX)->tf_regs.reg_rcx = (VALUE); } while (0)
#define SERVICE_ARG4_SET(CTX,VALUE) do { (CTX)->tf_regs.reg_rdi = (VALUE); } while (0)
#define SERVICE_ARG5_SET(CTX,VALUE) do { (CTX)->tf_regs.reg_rsi = (VALUE); } while (0)

void service_context_transfer(service_context_t dst, service_context_t src);

#endif
