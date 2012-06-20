#ifndef __PROC_ARCH_LOCAL_H__
#define __PROC_ARCH_LOCAL_H__

#define PLS_PREALLOC(type, name) __attribute__((section(".pls"))) type name
#define PLS_PTR_DEFINE(type, name, init)  __attribute__((section(".pls"))) type * name = (init)
#define PLS_PTR_DECLARE(type, name) extern type *name
#define PLS_ATOM_DEFINE(type, name, init)  __attribute__((section(".pls"))) type name = (init)
#define PLS_ATOM_DECLARE(type, name) extern type name

#define PLS(name) ({                                                \
            typeof(name) __result;                                      \
            __asm__ __volatile__("mov %%fs:(%1), %0" : "=r" (__result) : "r"(&name)) ;  \
            __result;                                                   \
        })
#define PLS_SET(name, value) do {                                   \
        typeof(name) __value = (value);                                 \
        __asm__ __volatile__("mov %1, %%fs:(%0)" : : "r"(&name), "r"(__value)); \
    } while (0)

int pls_init(void);

#include <types.h>

PLS_ATOM_DECLARE(uintptr_t, __pls_base);
#define PLS_PREALLOC_PTR(name) ((typeof(name) *)(PLS(__pls_base) + (uintptr_t)&name))

#endif
