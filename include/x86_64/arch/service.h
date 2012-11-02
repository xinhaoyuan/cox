#ifndef __ARCH_SERVICE_H__
#define __ARCH_SERVICE_H__

#include <types.h>

#define T_SERVICE 0x80

#ifndef __KERN__

#include <types.h>
#include <service.h>

typedef struct service_context_s *service_context_t;
typedef struct service_context_s
{
    uintptr_t args[6];
} service_context_s;

static __attribute__((always_inline)) __inline uintptr_t
__service_send(service_context_t ctx)
{
    uintptr_t result;
    __asm__ __volatile__("int %1"
                         : "=a"(result)
                         : "i" (T_SERVICE), "a"(ctx->args[0]), "b"(ctx->args[1]), "d"(ctx->args[2]), "c"(ctx->args[3]), "D"(ctx->args[4]), "S"(ctx->args[5])
                         : "cc", "memory");
    return result;
}

static __attribute__((always_inline)) __inline void
__service_listen(service_context_t ctx)
{
    __asm__ __volatile__("int %6"
                         : "=a"(ctx->args[0]), "=b"(ctx->args[1]), "=d"(ctx->args[2]), "=c"(ctx->args[3]), "=D"(ctx->args[4]), "=S"(ctx->args[5])
                         : "i" (T_SERVICE), "a"(0), "b"(SERVICE_SYS_LISTEN)
                         : "cc", "memory");
}

static __attribute__((always_inline)) __inline uintptr_t
__service_sys1(uintptr_t arg0)
{
    __asm__ __volatile__("int %1"
                         : "=a"(arg0)
                         : "i" (T_SERVICE), "a"(0), "b"(arg0)
                         : "cc", "memory");
    return arg0;
}

static __attribute__((always_inline)) __inline uintptr_t
__service_sys2(uintptr_t arg0, uintptr_t arg1)
{
    __asm__ __volatile__("int %1"
                         : "=a"(arg0)
                         : "i" (T_SERVICE), "a"(0), "b"(arg0), "d"(arg1)
                         : "cc", "memory");
    return arg0;
}

static __attribute__((always_inline)) __inline uintptr_t
__service_sys3(uintptr_t arg0, uintptr_t arg1, uintptr_t arg2)
{
    __asm__ __volatile__("int %1"
                         : "=a"(arg0)
                         : "i" (T_SERVICE), "a"(0), "b"(arg0), "d"(arg1), "c"(arg2)
                         : "cc", "memory");
    return arg0;
}

static __attribute__((always_inline)) __inline uintptr_t
__service_sys4(uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3)
{
    __asm__ __volatile__("int %1"
                         : "=a"(arg0)
                         : "i" (T_SERVICE), "a"(0), "b"(arg0), "d"(arg1), "c"(arg2), "D"(arg3)
                         : "cc", "memory");
    return arg0;
}

static __attribute__((always_inline)) __inline uintptr_t
__service_sys5(uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4)
{
    __asm__ __volatile__("int %1"
                         : "=a"(arg0)
                         : "i" (T_SERVICE), "a"(0), "b"(arg0), "d"(arg1), "c"(arg2), "D"(arg3), "S"(arg4)
                         : "cc", "memory");
    return arg0;
}

#endif

#endif
