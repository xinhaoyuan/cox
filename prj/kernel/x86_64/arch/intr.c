#include <types.h>
#include <cpu.h>
#include <proc.h>
#include <user.h>
#include <irq.h>
#include <lib/low_io.h>

#include "mem.h"
#include "lapic.h"
#include "intr.h"

static struct gatedesc idt[256] = {{0}};

struct pseudodesc idt_pd = {
    sizeof(idt) - 1, (uintptr_t)idt
};

void
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
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->tf_regs);
    cprintf("  trap 0x--------%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
    cprintf("  err  0x%016llx\n", tf->tf_err);
    cprintf("  rip  0x%016llx\n", tf->tf_rip);
    cprintf("  cs   0x------------%04x\n", tf->tf_cs);
    cprintf("  ds   0x------------%04x\n", tf->tf_ds);
    cprintf("  es   0x------------%04x\n", tf->tf_es);
    cprintf("  flag 0x%016llx\n", tf->tf_rflags);
    cprintf("  rsp  0x%016llx\n", tf->tf_rsp);
    cprintf("  ss   0x------------%04x\n", tf->tf_ss);

    int i, j;
    for (i = 0, j = 1; i < sizeof(IA32flags) / sizeof(IA32flags[0]); i ++, j <<= 1) {
        if ((tf->tf_rflags & j) && IA32flags[i] != NULL) {
            cprintf("%s,", IA32flags[i]);
        }
    }
    cprintf("IOPL=%d\n", (tf->tf_rflags & FL_IOPL_MASK) >> 12);
}

void
print_regs(struct pushregs *regs)
{
    cprintf("  rdi  0x%016llx\n", regs->reg_rdi);
    cprintf("  rsi  0x%016llx\n", regs->reg_rsi);
    cprintf("  rdx  0x%016llx\n", regs->reg_rdx);
    cprintf("  rcx  0x%016llx\n", regs->reg_rcx);
    cprintf("  rax  0x%016llx\n", regs->reg_rax);
    cprintf("  r8   0x%016llx\n", regs->reg_r8);
    cprintf("  r9   0x%016llx\n", regs->reg_r9);
    cprintf("  r10  0x%016llx\n", regs->reg_r10);
    cprintf("  r11  0x%016llx\n", regs->reg_r11);
    cprintf("  rbx  0x%016llx\n", regs->reg_rbx);
    cprintf("  rbp  0x%016llx\n", regs->reg_rbp);
    cprintf("  r12  0x%016llx\n", regs->reg_r12);
    cprintf("  r13  0x%016llx\n", regs->reg_r13);
    cprintf("  r14  0x%016llx\n", regs->reg_r14);
    cprintf("  r15  0x%016llx\n", regs->reg_r15);
}

static void
trap_dispatch(struct trapframe *tf)
{
	bool from_user = !trap_in_kernel(tf);
	if (from_user)
	{
		current->usr_thread->arch.tf = tf;
	}

	if (tf->tf_trapno < EXCEPTION_COUNT) {
		switch (tf->tf_trapno)
		{
		case T_PGFLT:
			pgflt_handler(tf->tf_err, __rcr2(), tf->tf_rip);
			break;
			
		default:
			print_trapframe(tf);
			cprintf("unhandled exception %d.\n", tf->tf_trapno);
			while (1) ;
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
		/* Kick to kernel, nothing to do (maybe sleep?) */
	}

	if (from_user)
	{
		__irq_enable();
		user_process_io(current);
		__irq_disable();
		user_before_return(current);
	}
}

void
trap(struct trapframe *tf)
{
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}
