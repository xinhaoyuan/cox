#ifndef __KERN_LWIP_CC_H__
#define __KERN_LWIP_CC_H__

#include <types.h>

typedef uint8_t u8_t;
typedef int8_t  s8_t;

typedef uint16_t u16_t;
typedef int16_t  s16_t;

typedef uint32_t u32_t;
typedef int32_t  s32_t;

typedef uintptr_t mem_ptr_t;

#define LWIP_ERR_T char 

#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define printf(x ...)  cprintf(x)
#define memset(x ...)  memset(x)
#define memmove(x ...) memmove(x)

#define LWIP_PLATFORM_DIAG(x ...)   cprintf(x)
#define LWIP_PLATFORM_ASSERT(x ...) cprintf(x)
#define U16_F "u"
#define S16_F "d"
#define X16_F "04x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "08x"
#define SZT_F "u"

#define BYTE_ORDER LITTLE_ENDIAN

/* #define SYS_ARCH_DECL_PROTECT(x) */
/* #define SYS_ARCH_PROTECT(x) */
/* #define SYS_ARCH_UNPROTECT(x) */

#define LWIP_PROVIDE_ERRNO

#endif
