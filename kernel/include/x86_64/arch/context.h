#ifndef __KERN_ARCH_CONTEXT_H__
#define __KERN_ARCH_CONTEXT_H__

#include <types.h>

typedef struct context_s
{
	uintptr_t stk_ptr;
	uintptr_t stk_top;

	uintptr_t pc;
} context_s;

typedef context_s *context_t;

void context_switch(context_t from, context_t to);
void context_fill(context_t ctx,
				  void (*entry)(void *arg), void *arg,
				  uintptr_t stk_top);

int context_init(void);

#endif
