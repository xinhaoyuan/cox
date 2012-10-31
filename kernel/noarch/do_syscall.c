#define DEBUG_COMPONENT DBG_IO

#include <debug.h>
#include <syscall.h>
#include <do_syscall.h>

void
do_syscall(syscall_context_t ctx)
{
    DEBUG("syscall %d.\n", SYSCALL_ARG0_GET(ctx));
    SYSCALL_ARG0_SET(ctx, 0);
}
