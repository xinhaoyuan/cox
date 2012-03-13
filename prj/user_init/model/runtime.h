#ifndef __RUNTIME_H__
#define __RUNTIME_H__

#include <types.h>
#include <user/tls.h>
#include <arch/context.h>

struct sched_node_s
{
	struct sched_node_s *prev, *next;
};

typedef struct sched_node_s sched_node_s;
typedef sched_node_s *sched_node_t;

typedef void (*fiber_entry_f) (void *args);

struct fiber_s
{
	context_s    ctx;
	fiber_entry_f entry;
	sched_node_s sched_node;
	int status;
};

#define FIBER_STATUS_RUNNABLE_IDLE   0
#define FIBER_STATUS_RUNNABLE_WEAK   1
#define FIBER_STATUS_RUNNABLE_STRONG 2
#define FIBER_STATUS_WAIT            3
#define FIBER_STATUS_ZOMBIE          4

typedef struct fiber_s fiber_s;
typedef fiber_s *fiber_t;

struct upriv_s
{
	fiber_s      idle_fiber;
	sched_node_s sched_node;
};

typedef struct upriv_s upriv_s;
typedef upriv_s *upriv_t;

#define TLS_IOCE_HEAD        (TLS_USTART)
#define TLS_IOCE_CAP         (TLS_USTART + 1)
#define TLS_IOCB_ENTRY       (TLS_USTART + 2)
#define TLS_IOCB_CAP         (TLS_USTART + 3)
#define TLS_IO_UBUSY         (TLS_USTART + 4)
#define TLS_CURRENT_FIBER    (TLS_USTART + 5)
#define TLS_UPRIV            (TLS_USTART + 6)

#define __proc_arg           (TLS(TLS_PROC_ARG))
#define __thread_arg         (TLS(TLS_THREAD_ARG))
#define __iocb_busy          (TLS(TLS_IOCB_BUSY))
#define __iocb_head          (TLS(TLS_IOCB_HEAD))
#define __iocb_tail          (TLS(TLS_IOCB_TAIL))
#define __ioce_head          ((io_call_entry_t)TLS(TLS_IOCE_HEAD))
#define __ioce_cap           (TLS(TLS_IOCE_CAP))
#define __iocb_entry         ((iobuf_index_t *)TLS(TLS_IOCB_ENTRY))
#define __iocb_cap           (TLS(TLS_IOCB_CAP))
#define __io_ubusy           (TLS(TLS_IO_UBUSY))
#define __current_fiber      ((fiber_t)TLS(TLS_CURRENT_FIBER))
#define __upriv              ((upriv_t)TLS(TLS_UPRIV))

void thread_init(tls_t tls);
void fiber_init(fiber_t fiber, fiber_entry_f entry, void *arg, void *stack, size_t stack_size);
void fiber_schedule(void);
void fiber_wait_try(void);
void fiber_notify(fiber_t f);

int io_save(void);
void io_restore(int status);

void debug_putchar(int ch);

#endif
