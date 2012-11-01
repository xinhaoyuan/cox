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

static __attribute__((always_inline)) __inline void
__service(service_context_t ctx)
{
    __asm__ __volatile__("int %6"
                         : "=a"(ctx->args[0]), "=b"(ctx->args[1]), "=d"(ctx->args[2]), "=c"(ctx->args[3]), "=D"(ctx->args[4]), "=S"(ctx->args[5])
                         : "i" (T_SERVICE), "a"(ctx->args[0]), "b"(ctx->args[1]), "d"(ctx->args[2]), "c"(ctx->args[3]), "D"(ctx->args[4]), "S"(ctx->args[5])
                         : "cc", "memory");
}

#endif

#endif
