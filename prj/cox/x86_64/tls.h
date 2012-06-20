#ifndef __COX_ARCH_TLS_H__
#define __COX_ARCH_TLS_H__

#include <uio.h>

struct tls_s
{
    struct
    {
        uintptr_t khead;
        uintptr_t utail;
    } iocr_ctl;
        
    struct
    {
        uintptr_t uhead;
        uintptr_t ktail;
    } iocb_ctl;

    /* start of user defined data */
    uintptr_t data[0];

    /* not used by kernel. This area should be overlayed by user */
    struct
    {
        uintptr_t arg0;
        uintptr_t arg1;
        uintptr_t stack_ptr;
        
        iobuf_index_t   io_cap;
        io_call_entry_t ioce;
        iobuf_index_t  *iocr;
        iobuf_index_t  *iocb;
    } info;
};

typedef struct tls_s  tls_s;
typedef struct tls_s *tls_t;

#include <asm/mach.h>

static char tls_data_size_assert[sizeof(tls_s) > _MACH_PAGE_SIZE ? -1 : 0] __attribute__((unused));

/* The flat position of each variable in tls */
#define TLS_IOCR_KHEAD 0
#define TLS_IOCR_UTAIL 1
#define TLS_IOCB_UHEAD 2
#define TLS_IOCB_KTAIL 3
#define TLS_USTART     4

#define TLS(index) ({                                                   \
            uintptr_t __result;                                         \
            __asm__ __volatile__("mov %%gs:(,%1,8), %0" : "=r" (__result) : "r"(index)) ; \
            __result;                                                   \
        })
#define TLS_SET(index, value) do {                                      \
        uintptr_t __value = (value);                                    \
        __asm__ __volatile__("mov %1, %%gs:(,%0,8)" : : "r"(index), "r"(__value)); \
    } while (0)


#endif
