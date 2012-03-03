#ifndef __PROC_ARCH_LOCAL_H__
#define __PROC_ARCH_LOCAL_H__

#define PLS_PTR_DEFINE(type, name, init)  __attribute__((section(".pls"))) type * name = (init)
#define PLS_PTR_DECLARE(type, name) extern type *name
#define PLS_PTR(name) ({											\
			typeof(name) __result;										\
			__asm__ __volatile__("movq %%fs:(%1), %0" : "=r" (__result) : "r"(&name)) ;	\
			__result;														\
		})
#define PLS_PTR_SET(name, value) do {									\
		typeof(name) __value = (value);									\
		__asm__ __volatile__("movq %1, %%fs:(%0)" : : "r"(&name), "r"(__value)); \
	} while (0)

void pls_init(void);

#endif
