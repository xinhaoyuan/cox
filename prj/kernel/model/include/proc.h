#ifndef __KERN_MODEL_PROC_H__
#define __KERN_MODEL_PROC_H__

#include <types.h>
#include <arch/context.h>
#include <arch/local.h>
#include <sync/spinlock.h>

/* ==  SCHEDULE CLASS INTERFACE  ============================== */

typedef struct sched_node_s *sched_node_t;
typedef struct sched_node_s
{
	struct sched_class_s *class;
	struct
	{
		sched_node_t prev, next;
	} rr;
} sched_node_s;

typedef struct runqueue_s *runqueue_t;
typedef struct runqueue_s
{
	spinlock_s lock;
	
	struct
	{
		sched_node_t node;
	} idle;
	
	struct
	{
		sched_node_t first;
	} rr;
} runqueue_s;

/* Func in sched_class can be called only with rq->lock acquired */
typedef struct sched_class_s *sched_class_t;
typedef struct sched_class_s 
{
	void (*init)(runqueue_t rq);
	void (*enqueue)(runqueue_t rq, sched_node_t node);
	void (*dequeue)(runqueue_t rq, sched_node_t node);
	sched_node_t (*pick)(runqueue_t rq);
} sched_class_s;

#define SCHED_CLASS_COUNT 2
#define SCHED_CLASS_IDLE  0
#define SCHED_CLASS_RR    1
sched_class_t sched_class[SCHED_CLASS_COUNT];

/* ==  PROC STRUCTURE  ======================================== */

#define PROC_NAME_MAX 16

typedef struct proc_s *proc_t;
typedef struct proc_s
{
	char name[PROC_NAME_MAX];
	
	spinlock_s lock;
	int status;
	
	context_s ctx;
	sched_node_s sched_node;

	proc_t sched_prev;
} proc_s;

#define PROC_STATUS_RUNNABLE_WEAK   0
#define PROC_STATUS_RUNNABLE_STRONG 1
#define PROC_STATUS_WAITING         2

PLS_PTR_DECLARE(proc_s, __current);
#define current (PLS(__current))

int  proc_init(proc_t proc, const char *name, int class, void (*entry)(void *arg), void *arg, uintptr_t stack_top);
void proc_wait_pretend(void);
void proc_wait_try(void);
void proc_notify(proc_t proc);

/* ==  SCHEDULER  ============================================= */

int sched_init(void);
int sched_init_mp(void);
void schedule(void);

#endif
