#ifndef __USER_ARCH_SYSCALL__
#define __USER_ARCH_SYSCALL__

#include <types.h>

#define T_SYSCALL               0x80

#ifndef __KERN__

#include <user/io.h>

static __inline void __io(void) __attribute__((always_inline));

static __inline void __io(void)
{ __asm __volatile ("int %0": : "i" (T_SYSCALL), "a"(0) : "cc", "memory"); }

static __inline void __debug_putc(int ch) __attribute__((always_inline));

static __inline void __debug_putc(int ch)
{ __asm __volatile ("int %0": : "i" (T_SYSCALL), "a"((uintptr_t)1), "b"((uintptr_t)ch) : "cc", "memory"); }

#endif


#endif
