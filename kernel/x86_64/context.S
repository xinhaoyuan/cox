#include <arch/memlayout.h>

              .text
              .global __context_deadend
__context_deadend:
              movq  %ss:0x08(%rsp), %rdi
              movq  $0, %ss:0(%rsp)
              jmp   context_deadend

              .global __context_init
__context_init:
              movq  %ss:0x10(%rsp), %rdi
              movq  $0, %rbp
              retq

              .global __context_switch

__context_switch:

# Search about the calling convention of X86_64
# (%rdi, %rsi, %rdx, %rcx)

              pushfq
# no pusha in x86_64
#if CONTEXT_SWITCH_FEW_SAVE
              pushq %rbp
#else
              pushq %rdi
              pushq %rsi
              pushq %rdx
              pushq %rcx
              pushq %rax
              pushq %r8
              pushq %r9
              pushq %r10
              pushq %r11
              pushq %rbx
              pushq %rbp
              pushq %r12
              pushq %r13
              pushq %r14
              pushq %r15
#endif

              movq  %rsp, 0(%rdi)
              movabs $__context_switch_ret, %rax
              movq  %rax, 0(%rsi)
              movq  %rdx, %rsp
              jmpq  *%rcx

__context_switch_ret:
#if CONTEXT_SWITCH_FEW_SAVE
              popq  %rbp
#else
              popq  %r15
              popq  %r14
              popq  %r13
              popq  %r12
              popq  %rbp
              popq  %rbx
              popq  %r11
              popq  %r10
              popq  %r9
              popq  %r8
              popq  %rax
              popq  %rcx
              popq  %rdx
              popq  %rsi
              popq  %rdi
#endif
              popfq
              retq

