#ifndef __USER_TLS_H__
#define __USER_TLS_H__

#include <user/io.h>

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

#include <mach.h>
static char tls_data_size_assert[sizeof(tls_s) > _MACH_PAGE_SIZE ? -1 : 0] __attribute__((unused));

/* The flat position of each variable in tls */
#define TLS_IOCR_KHEAD 0
#define TLS_IOCR_UTAIL 1
#define TLS_IOCB_UHEAD 2
#define TLS_IOCB_KTAIL 3
#define TLS_USTART     4

#endif
