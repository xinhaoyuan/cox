#ifndef __ARCH_STD_TYPES_H__
#define __ARCH_STD_TYPES_H__

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef int bool;
#define FALSE 0
#define TRUE  1

#define CHAR_BITS       8
#define PTR_BITS        64

typedef char               int8_t;
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef long long          int64_t;
typedef unsigned long long uint64_t;

typedef int8_t   s8_t;
typedef uint8_t  u8_t;
typedef int16_t  s16_t;
typedef uint16_t u16_t;
typedef int32_t  s32_t;
typedef uint32_t u32_t;
typedef int64_t  s64_t;
typedef uint64_t u64_t;

typedef int64_t  intptr_t;
typedef uint64_t uintptr_t;

typedef uintptr_t size_t;
typedef intptr_t  off_t;

/* Return the offset of 'member' relative to the beginning of a struct type */
#define OFFSET_OF(type, member)                                      \
    ((size_t)(&((type *)0)->member))

/* *
 * CONTAINER_OF - get the struct from a ptr
 * @ptr:    a struct pointer of member
 * @type:   the type of the struct this is embedded in
 * @member: the name of the member within the struct
 * */
#define CONTAINER_OF(ptr, type, member)                 \
    ((type *)((char *)(ptr) - OFFSET_OF(type, member)))

#endif
