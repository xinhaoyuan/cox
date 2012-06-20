#ifndef __COX_ARCH_SYSCALL__
#define __COX_ARCH_SYSCALL__

#include <types.h>

#define T_SYSCALL 0x80

#define T_SYSCALL_IO_FLUSH   0
#define T_SYSCALL_IO_URGENT  1
#define T_SYSCALL_DEBUG_PUTC 2
#define T_SYSCALL_GET_TICK   3

#ifndef __KERN__

#include <user/io.h>
#define SYSENTRY static __attribute__((always_inline)) __inline

SYSENTRY void
__io_flush(void)
{ __asm __volatile ("int %0": : "i" (T_SYSCALL), "a"((uintptr_t)T_SYSCALL_IO_FLUSH) : "cc", "memory"); }

SYSENTRY void
__io_urgent(int idx)
{ __asm __volatile ("int %0": : "i" (T_SYSCALL), "a"((uintptr_t)T_SYSCALL_IO_URGENT), "b"((uintptr_t)idx) : "cc", "memory"); }

SYSENTRY void
__debug_putc(int ch)
{ __asm __volatile ("int %0": : "i" (T_SYSCALL), "a"((uintptr_t)T_SYSCALL_DEBUG_PUTC), "b"((uintptr_t)ch) : "cc", "memory"); }

SYSENTRY uintptr_t
__get_tick(void)
{
    uintptr_t result;
    __asm __volatile ("int %1": "=a"(result) : "i" (T_SYSCALL), "a"((uintptr_t)T_SYSCALL_GET_TICK) : "cc", "memory");
    return result;
}

#endif


#endif
