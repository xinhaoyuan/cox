#ifndef __USER_TLS_H__
#define __USER_TLS_H__

#include <user/io.h>

struct tls_s
{
    uintptr_t proc_arg;
    uintptr_t thread_arg;

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
        iobuf_index_t   io_cap;
        io_call_entry_t ioce;
        iobuf_index_t  *iocr;
        iobuf_index_t  *iocb;
    } info;
};

typedef struct tls_s  tls_s;
typedef struct tls_s *tls_t;

/* The flat position of each variable in tls */
#define TLS_PROC_ARG   0
#define TLS_THREAD_ARG 1
#define TLS_IOCR_KHEAD 2
#define TLS_IOCR_UTAIL 3
#define TLS_IOCB_UHEAD 4
#define TLS_IOCB_KTAIL 5
#define TLS_USTART     6

#endif
