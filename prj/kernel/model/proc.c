#include <proc.h>

PLS_PTR_DEFINE(proc_s, __current, NULL);
#define current_set(proc) PLS_PTR_SET(__current, proc)

/* Call arch func to do the switch */
static inline void
proc_do_switch(proc_t prev, proc_t proc)
{
	context_t from = NULL;
	if (prev != NULL)
		from = &prev->ctx;
	context_switch(from, &proc->ctx);
}

/* Caller should hold the proc lock of ``current'' and ``proc'' */
void
proc_switch(proc_t proc)
{
	proc_t prev = current;
	current_set(proc);
	
	proc_do_switch(prev, proc);
}
