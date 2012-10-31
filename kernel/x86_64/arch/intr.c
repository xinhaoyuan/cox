#define DEBUG_COMPONENT DBG_IO

#include <types.h>
#include <asm/cpu.h>
#include <irq.h>
#include <debug.h>
#include <arch/local.h>
#include <proc.h>
#include <do_syscall.h>
#include <user.h>

#include "mem.h"
#include "lapic.h"
#include "intr.h"

static struct gatedesc idt[256] = {{0}};

struct pseudodesc idt_pd = {
    sizeof(idt) - 1, (uintptr_t)idt
};

int
idt_init(void) {
    extern uintptr_t __vectors[];
    int i;
    for (i = 0; i < sizeof(idt) / sizeof(struct gatedesc); i ++) {
        SETGATE(idt[i], 0, GD_KTEXT, __vectors[i], DPL_KERNEL);
    }
    
    SETGATE(idt[T_SYSCALL], 0, GD_KTEXT, __vectors[T_SYSCALL], DPL_USER);
    SETGATE(idt[T_IPI], 0, GD_KTEXT, __vectors[T_IPI], DPL_USER);
    SETGATE(idt[T_IPI_DOS], 0, GD_KTEXT, __vectors[T_IPI_DOS], DPL_USER);

    /* Load IDT for BOOT CPU */
    __lidt(&idt_pd);
    return 0;
}

static const char *
trapname(int trapno) {
    static const char * const excnames[] = {
        "Divide error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection",
        "Page Fault",
        "(unknown trap)",
        "x87 FPU Floating-Point Error",
        "Alignment Check",
        "Machine-Check",
        "SIMD Floating-Point Exception"
    };

    if (trapno < sizeof(excnames)/sizeof(const char * const)) {
        return excnames[trapno];
    }
    if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16) {
        return "Hardware Interrupt";
    }
    return "(unknown trap)";
}

bool
trap_in_kernel(struct trapframe *tf) {
    return (tf->tf_cs == (uint16_t)KERNEL_CS);
}

static const char *IA32flags[] = {
    "CF", NULL, "PF", NULL, "AF", NULL, "ZF", "SF",
    "TF", "IF", "DF", "OF", NULL, NULL, "NT", NULL,
    "RF", "VM", "AC", "VIF", "VIP", "ID", NULL, NULL,
};

void
print_trapframe(struct trapframe *tf)
{
    DEBUG("trapframe at %p\n", tf);
    print_regs(&tf->tf_regs);
    DEBUG("  trap 0x--------%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
    DEBUG("  err  0x%016llx\n", tf->tf_err);
    DEBUG("  rip  0x%016llx\n", tf->tf_rip);
    DEBUG("  cs   0x------------%04x\n", tf->tf_cs);
    DEBUG("  ds   0x------------%04x\n", tf->tf_ds);
    DEBUG("  es   0x------------%04x\n", tf->tf_es);
    DEBUG("  flag 0x%016llx\n", tf->tf_rflags);
    DEBUG("  rsp  0x%016llx\n", tf->tf_rsp);
    DEBUG("  ss   0x------------%04x\n", tf->tf_ss);

    int i, j;
    for (i = 0, j = 1; i < sizeof(IA32flags) / sizeof(IA32flags[0]); i ++, j <<= 1) {
        if ((tf->tf_rflags & j) && IA32flags[i] != NULL) {
            DEBUG("%s,", IA32flags[i]);
        }
    }
    DEBUG("IOPL=%d\n", (tf->tf_rflags & FL_IOPL_MASK) >> 12);
}

void
print_regs(struct pushregs *regs)
{
    DEBUG("  rdi  0x%016llx\n", regs->reg_rdi);
    DEBUG("  rsi  0x%016llx\n", regs->reg_rsi);
    DEBUG("  rdx  0x%016llx\n", regs->reg_rdx);
    DEBUG("  rcx  0x%016llx\n", regs->reg_rcx);
    DEBUG("  rax  0x%016llx\n", regs->reg_rax);
    DEBUG("  r8   0x%016llx\n", regs->reg_r8);
    DEBUG("  r9   0x%016llx\n", regs->reg_r9);
    DEBUG("  r10  0x%016llx\n", regs->reg_r10);
    DEBUG("  r11  0x%016llx\n", regs->reg_r11);
    DEBUG("  rbx  0x%016llx\n", regs->reg_rbx);
    DEBUG("  rbp  0x%016llx\n", regs->reg_rbp);
    DEBUG("  r12  0x%016llx\n", regs->reg_r12);
    DEBUG("  r13  0x%016llx\n", regs->reg_r13);
    DEBUG("  r14  0x%016llx\n", regs->reg_r14);
    DEBUG("  r15  0x%016llx\n", regs->reg_r15);
}

PLS_ATOM_DECLARE(int, __local_irq_save);

static void
trap_dispatch(struct trapframe *tf)
{
    proc_t p = current;
    bool from_user = !trap_in_kernel(tf);
    
    if (from_user)
    {
        USER_THREAD(p)->arch.tf = tf;
        user_thread_after_leave(p);
    }

    if (tf->tf_trapno < EXCEPTION_COUNT) {
        switch (tf->tf_trapno)
        {
        case T_PGFLT:
            pgflt_handler(tf->tf_err, __rcr2(), tf->tf_rip);
            break;
            
        default:
            print_trapframe(tf);
            PANIC("unhandled exception %d.\n", tf->tf_trapno);
        }
    }
    else if (tf->tf_trapno >= IRQ_OFFSET &&
             tf->tf_trapno <  IRQ_OFFSET + IRQ_COUNT)
    {
        lapic_eoi_send();
        irq_entry(tf->tf_trapno - IRQ_OFFSET);
    }
    else if (tf->tf_trapno == T_SYSCALL)
    {
        do_syscall(tf);
    }

    if (from_user)
    {
#if 0        
        __irq_enable();
        user_process_io(p);
        __irq_disable();
#endif
        
        if (PLS(__local_irq_save) != 0) DEBUG("WARNING: return to user with irq saved\n");
        user_thread_before_return(p);
    }
}

void
trap(struct trapframe *tf)
{
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}
