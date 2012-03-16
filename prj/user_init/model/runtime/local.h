#ifndef __LOCAL_H__
#define __LOCAL_H__

#include <types.h>
#include <user/tls.h>
#include <user/local.h>

struct fiber_s;
struct upriv_s;

#define TLS_IOCE_HEAD        (TLS_USTART)
#define TLS_IOCE_CAP         (TLS_USTART + 1)
#define TLS_IOCB_ENTRY       (TLS_USTART + 2)
#define TLS_IOCB_CAP         (TLS_USTART + 3)
#define TLS_IO_UBUSY         (TLS_USTART + 4)
#define TLS_CURRENT_FIBER    (TLS_USTART + 5)
#define TLS_UPRIV            (TLS_USTART + 6)

#define __proc_arg           (TLS(TLS_PROC_ARG))
#define __thread_arg         (TLS(TLS_THREAD_ARG))
#define __iocb_busy          ((iobuf_index_t)TLS(TLS_IOCB_BUSY))
#define __iocb_head          ((iobuf_index_t)TLS(TLS_IOCB_HEAD))
#define __iocb_tail          ((iobuf_index_t)TLS(TLS_IOCB_TAIL))
#define __ioce_head          ((io_call_entry_t)TLS(TLS_IOCE_HEAD))
#define __ioce_cap           ((iobuf_index_t)TLS(TLS_IOCE_CAP))
#define __iocb_entry         ((iobuf_index_t *)TLS(TLS_IOCB_ENTRY))
#define __iocb_cap           ((iobuf_index_t)TLS(TLS_IOCB_CAP))
#define __io_ubusy           ((int)TLS(TLS_IO_UBUSY))
#define __current_fiber      ((struct fiber_s *)TLS(TLS_CURRENT_FIBER))
#define __upriv              ((struct upriv_s *)TLS(TLS_UPRIV))

inline static void __iocb_busy_set(iobuf_index_t v) __attribute__((always_inline));
inline static void __iocb_busy_set(iobuf_index_t v) { TLS_SET(TLS_IOCB_BUSY,(uintptr_t)(v)); }
inline static void __iocb_head_set(iobuf_index_t v) __attribute__((always_inline));
inline static void __iocb_head_set(iobuf_index_t v) { TLS_SET(TLS_IOCB_HEAD,(uintptr_t)(v)); }
inline static void __iocb_tail_set(iobuf_index_t v) __attribute__((always_inline));
inline static void __iocb_tail_set(iobuf_index_t v) { TLS_SET(TLS_IOCB_TAIL,(uintptr_t)(v)); }
inline static void __ioce_head_set(io_call_entry_t v) __attribute__((always_inline));
inline static void __ioce_head_set(io_call_entry_t v) { TLS_SET(TLS_IOCE_HEAD,(uintptr_t)(v)); }
inline static void __ioce_cap_set(iobuf_index_t v) __attribute__((always_inline));
inline static void __ioce_cap_set(iobuf_index_t v) { TLS_SET(TLS_IOCE_CAP,(uintptr_t)(v)); }
inline static void __iocb_entry_set(iobuf_index_t *v) __attribute__((always_inline));
inline static void __iocb_entry_set(iobuf_index_t *v) { TLS_SET(TLS_IOCB_ENTRY,(uintptr_t)(v)); }
inline static void __iocb_cap_set(iobuf_index_t v) __attribute__((always_inline));
inline static void __iocb_cap_set(iobuf_index_t v) { TLS_SET(TLS_IOCB_CAP,(uintptr_t)(v)); }
inline static void __io_ubusy_set(int v) __attribute__((always_inline));
inline static void __io_ubusy_set(int v) { TLS_SET(TLS_IO_UBUSY,(uintptr_t)(v)); }
inline static void __current_fiber_set(struct fiber_s *v) __attribute__((always_inline));
inline static void __current_fiber_set(struct fiber_s *v) { TLS_SET(TLS_CURRENT_FIBER,(uintptr_t)(v)); }
inline static void __upriv_set(struct upriv_s *v) __attribute__((always_inline));
inline static void __upriv_set(struct upriv_s *v) { TLS_SET(TLS_UPRIV,(uintptr_t)(v)); }

void thread_init(tls_t tls);

#endif
