#ifndef __LOCAL_H__
#define __LOCAL_H__

#include <types.h>
#include <user/tls.h>
#include <user/arch/local.h>

struct fiber_s;
struct upriv_s;

#define TLS_IO_CAP           (TLS_USTART)
#define TLS_IOCE             (TLS_USTART + 1)
#define TLS_IOCE_FREE        (TLS_USTART + 2)
#define TLS_IOCR             (TLS_USTART + 3)
#define TLS_IOCB             (TLS_USTART + 4)
#define TLS_CURRENT_FIBER    (TLS_USTART + 5)
#define TLS_UPRIV            (TLS_USTART + 6)

#define __iocr_khead         ((iobuf_index_t)TLS(TLS_IOCR_KHEAD))
#define __iocr_utail         ((iobuf_index_t)TLS(TLS_IOCR_UTAIL))
#define __iocb_uhead         ((iobuf_index_t)TLS(TLS_IOCB_UHEAD))
#define __iocb_ktail         ((iobuf_index_t)TLS(TLS_IOCB_KTAIL))
#define __io_cap             ((iobuf_index_t)TLS(TLS_IO_CAP))
#define __ioce               ((io_call_entry_t)TLS(TLS_IOCE))
#define __ioce_free          ((iobuf_index_t)TLS(TLS_IOCE_FREE))
#define __iocr               ((iobuf_index_t *)TLS(TLS_IOCR))
#define __iocb               ((iobuf_index_t *)TLS(TLS_IOCB))
#define __current_fiber      ((struct fiber_s *)TLS(TLS_CURRENT_FIBER))
#define __upriv              ((struct upriv_s *)TLS(TLS_UPRIV))

__attribute__((always_inline)) inline static void __iocr_utail_set(iobuf_index_t v)
{ TLS_SET(TLS_IOCR_UTAIL, (uintptr_t)(v)); }
__attribute__((always_inline)) inline static void __iocb_uhead_set(iobuf_index_t v)
{ TLS_SET(TLS_IOCB_UHEAD, (uintptr_t)(v)); }
__attribute__((always_inline)) inline static void __io_cap_set(iobuf_index_t v)
{ TLS_SET(TLS_IO_CAP, (uintptr_t)(v)); }
__attribute__((always_inline)) inline static void __ioce_set(io_call_entry_t ioce)
{ TLS_SET(TLS_IOCE, (uintptr_t)ioce); }
__attribute__((always_inline)) inline static void __ioce_free_set(iobuf_index_t v)
{ TLS_SET(TLS_IOCE_FREE, (uintptr_t)(v)); }
__attribute__((always_inline)) inline static void __iocr_set(iobuf_index_t *iocr)
{ TLS_SET(TLS_IOCR, (uintptr_t)iocr); }
__attribute__((always_inline)) inline static void __iocb_set(iobuf_index_t *iocb)
{ TLS_SET(TLS_IOCB, (uintptr_t)iocb); }
__attribute__((always_inline)) inline static void __current_fiber_set(struct fiber_s *v)
{ TLS_SET(TLS_CURRENT_FIBER, (uintptr_t)(v)); }
__attribute__((always_inline)) inline static void __upriv_set(struct upriv_s *v)
{ TLS_SET(TLS_UPRIV, (uintptr_t)(v)); }

void thread_init(tls_t tls);

#endif
