#ifndef __ARCH_LOCAL_H__
#define __ARCH_LOCAL_H__

#include <types.h>

#define TLS(index) ({													\
			uintptr_t __result;											\
			__asm__ __volatile__("mov %%gs:(,%1,8), %0" : "=r" (__result) : "r"(index)) ; \
			__result;													\
		})
#define TLS_SET(index, value) do {										\
		uintptr_t __value = (value);									\
		__asm__ __volatile__("mov %1, %%gs:(,%0,8)" : : "r"(index), "r"(__value)); \
	} while (0)

#endif
