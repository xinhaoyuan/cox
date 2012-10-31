#ifndef __KERN_ARCH_TRAPFRAME_H__
#define __KERN_ARCH_TRAPFRAME_H__

/* registers as pushed by pushal */
struct pushregs {
    uint64_t reg_r15;
    uint64_t reg_r14;
    uint64_t reg_r13;
    uint64_t reg_r12;
    uint64_t reg_rbp;
    uint64_t reg_rbx;
    uint64_t reg_r11;
    uint64_t reg_r10;
    uint64_t reg_r9;
    uint64_t reg_r8;
    uint64_t reg_rax;
    uint64_t reg_rcx;
    uint64_t reg_rdx;
    uint64_t reg_rsi;
    uint64_t reg_rdi;
};

struct trapframe {
    uint16_t tf_ds;
    uint16_t tf_padding0[3];
    uint16_t tf_es;
    uint16_t tf_padding1[3];
    struct pushregs tf_regs;
    uint64_t tf_trapno;
    /* below here defined by x86 hardware */
    uint64_t tf_err;
    uintptr_t tf_rip;
    uint16_t tf_cs;
    uint16_t tf_padding2[3];
    uint64_t tf_rflags;
    uintptr_t tf_rsp;
    uint16_t tf_ss;
    uint16_t tf_padding3[3];
} __attribute__((packed));

#endif
