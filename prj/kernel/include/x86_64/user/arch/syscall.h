#ifndef __USER_ARCH_SYSCALL__
#define __USER_ARCH_SYSCALL__

#include <types.h>

#define T_SYSCALL 0x80

#define T_SYSCALL_IO_FLUSH   0
#define T_SYSCALL_DEBUG_PUTC 1
#define T_SYSCALL_GET_TICK   2

#ifndef __KERN__

#include <user/io.h>

static __inline void __io_flush(void) __attribute__((always_inline));

static __inline void __io_flush(void)
{ __asm __volatile ("int %0": : "i" (T_SYSCALL), "a"((uintptr_t)T_SYSCALL_IO_FLUSH) : "cc", "memory"); }

static __inline void __debug_putc(int ch) __attribute__((always_inline));

static __inline void __debug_putc(int ch)
{ __asm __volatile ("int %0": : "i" (T_SYSCALL), "a"((uintptr_t)T_SYSCALL_DEBUG_PUTC), "b"((uintptr_t)ch) : "cc", "memory"); }

static __inline uintptr_t __get_tick(void) __attribute__((always_inline));

static __inline uintptr_t __get_tick(void)
{
    uint64_t result;
    __asm __volatile ("int %1": "=a"(result) : "i" (T_SYSCALL), "a"((uintptr_t)T_SYSCALL_GET_TICK) : "cc", "memory");
    return result;
}


#endif


#endif
