#include <lib/low_io.h>
#include <arch/context.h>

extern void __context_switch(uintptr_t *from_esp, uintptr_t *from_pc, uintptr_t to_esp, uintptr_t to_pc);
extern void __context_init(void);
extern void __context_deadend(void);

#define __TEST 0

#if __TEST

#include <arch/mem.h>
#include <mmu.h>

context_s ca, cb;

static void
ea(void *arg)
{
	cprintf("EA %p\n", arg);
	context_switch(&ca, &cb);
}

static void
eb(void *arg)
{
	cprintf("EB %p\n", arg);
	context_switch(&cb, &ca);
}
#endif
int
context_init(void)
{
#if __TEST
	/* Simple test */
	cprintf("CONTEXT %p %p\n", &ca, &cb);

	context_fill(&ca, ea, (void *)0xdead, (uintptr_t)page_alloc(1) + PGSIZE);
	context_fill(&cb, eb, (void *)0x1234, (uintptr_t)page_alloc(1) + PGSIZE);
	
	context_switch(NULL, &ca);	
#endif
	
	return 0;
}

void
context_deadend(context_t ctx)
{
	cprintf("CONTEXT %p FALL INTO DEAD END\n", ctx);
	while (1) ;
}

void
context_fill(context_t ctx, void (*entry)(void *arg), void *arg, uintptr_t stk_top)
{
	ctx->stk_top = stk_top;
	ctx->stk_ptr = ctx->stk_top;
	 
	ctx->stk_ptr -= sizeof(void *);
	*(void **)ctx->stk_ptr = ctx;

	ctx->stk_ptr -= sizeof(void *);
	*(void **)ctx->stk_ptr = arg;

	ctx->stk_ptr -= sizeof(void *);
	*(void **)ctx->stk_ptr = &__context_deadend;

	ctx->stk_ptr -= sizeof(void *);
	*(void **)ctx->stk_ptr = entry;

	ctx->pc = (uintptr_t)__context_init;
}

void
context_switch(context_t old, context_t to)
{
	if (old != NULL)
	{
		__context_switch(&old->stk_ptr, &old->pc, to->stk_ptr, to->pc);
	}
	else
	{
		uintptr_t __unused;
		__context_switch(&__unused, &__unused, to->stk_ptr, to->pc);
	}
}
