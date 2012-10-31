#ifndef __KERN_ARCH_CPU_H__
#define __KERN_ARCH_CPU_H__

#include <types.h>

void cpu_init(void) __attribute__((noreturn));
void cpu_set_trap_stacktop(uintptr_t stacktop);

#endif
