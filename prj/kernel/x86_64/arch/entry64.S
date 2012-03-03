#include <arch/mmu.h>
#include <arch/memlayout.h>

.text
.code64
.global kern_entry64
kern_entry64:
    movq $0x0, %rbp
    movq $KERNBASE, %rsp
    addq $bootstacktop - KERNBASE, %rsp
    call __kern_early_init

# should never get here
spin:
    jmp spin

.data
.align PGSIZE
    .globl bootstack
bootstack:
    .space KERN_INIT_STACK_SIZE
    .globl bootstacktop
bootstacktop:

