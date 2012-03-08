#ifndef __ARCH_SYSCALL__
#define __ARCH_SYSCALL__

#define T_SYSCALL               0x80

#ifndef __KERN__

#include <user/io.h>

static __inline void __io(void) __attribute__((always_inline));

static __inline void __io(void)
{ __asm __volatile ("int %0": : "i" (T_SYSCALL) : "cc", "memory"); }

#endif


#endif
