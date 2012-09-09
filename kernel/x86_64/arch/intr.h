#ifndef __KERN_ARCH_INTR_H__
#define __KERN_ARCH_INTR_H__

#include <types.h>
#include <asm/cpu.h>
#include <asm/mmu.h>
#include <arch/irq.h>
#include <syscall.h>

/* Trap Numbers */
#define T_IPI                   0x81
#define T_IPI_DOS               0x82


/* Processor-defined: */
#define T_DIVIDE                0   // divide error
#define T_DEBUG                 1   // debug exception
#define T_NMI                   2   // non-maskable interrupt
#define T_BRKPT                 3   // breakpoint
#define T_OFLOW                 4   // overflow
#define T_BOUND                 5   // bounds check
#define T_ILLOP                 6   // illegal opcode
#define T_DEVICE                7   // device not available
#define T_DBLFLT                8   // double fault
// #define T_COPROC             9   // reserved (not used since 486)
#define T_TSS                   10  // invalid task switch segment
#define T_SEGNP                 11  // segment not present
#define T_STACK                 12  // stack exception
#define T_GPFLT                 13  // general protection fault
#define T_PGFLT                 14  // page fault
// #define T_RES                15  // reserved
#define T_FPERR                 16  // floating point error
#define T_ALIGN                 17  // aligment check
#define T_MCHK                  18  // machine check
#define T_SIMDERR               19  // SIMD floating point error
#define EXCEPTION_COUNT         32

/* Hardware IRQ numbers. We receive these as (IRQ_OFFSET + IRQ_xx) */
#define IRQ_OFFSET              32  // IRQ 0 corresponds to int IRQ_OFFSET

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

void idt_init(void);
void print_trapframe(struct trapframe *tf);
void print_regs(struct pushregs *regs);
bool trap_in_kernel(struct trapframe *tf);

extern struct pseudodesc idt_pd;

#endif