#ifndef __USER_IOCB_H__
#define __USER_IOCB_H__

#ifndef __KERN__

static inline void iocb_return(void *ret) __attribute__((always_inline));

static inline void
iocb_return(void *ret)
{
	asm volatile("movq %0, %%rsp\n"
				 /* dummy ds pop */
				 "popq %%rax\n"
				 /* dummy es pop */
				 "popq %%rax\n"
				 /* regs pop */
				 "popq %%r15\n"
				 "popq %%r14\n"
				 "popq %%r13\n"
				 "popq %%r12\n"
				 "popq %%rbp\n"
				 "popq %%rbx\n"
				 "popq %%r11\n"
				 "popq %%r10\n"
				 "popq %%r9\n"
				 "popq %%r8\n"
				 "popq %%rax\n"
				 "popq %%rcx\n"
				 "popq %%rdx\n"
				 "popq %%rsi\n"
				 "popq %%rdi\n"
				 /* get rid of the trap number and error code */
				 "addq $0x10, %%rsp\n"
				 /* user level iretq */
				 "iretq\n" : : "r"(ret));
}

#endif

#endif
