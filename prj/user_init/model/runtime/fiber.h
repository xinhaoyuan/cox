#ifndef __FIBER_H__
#define __FIBER_H__

#include <arch/context.h>
#include "sync.h"

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
	semaphore_s  io_sem;
};

typedef struct upriv_s upriv_s;
typedef upriv_s *upriv_t;

void fiber_init(fiber_t fiber, fiber_entry_f entry, void *arg, void *stack, size_t stack_size);
void fiber_schedule(void);
void fiber_wait_try(void);
void fiber_notify(fiber_t f);

#endif
