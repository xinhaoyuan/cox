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
	sched_node_t prev, next;
} sched_node_s;

typedef struct sched_class_s *sched_class_t;
typedef struct sched_class_s 
{
	void(*init)(sched_node_t init_node);
	void(*enqueue)(sched_node_t node);
	void(*dequeue)(sched_node_t node);
	void(*pick)(sched_node_t node);
} sched_class_s;

#define SCHED_CLASS_COUNT 1
#define SCHED_CLASS_RR    0
sched_class_s sched_class[SCHED_CLASS_COUNT];

void sched_class_init(void);

/* ==  PROC STRUCTURE  ======================================== */

typedef struct proc_s *proc_t;
typedef struct proc_s
{
	spinlock_s lock;
	int status;
	
	context_s ctx;
	sched_node_s sched_node;
	sched_class_t sched_class;
} proc_s;

#define PROC_STATUS_RUN_WEAK   0
#define PROC_STATUS_RUN_STRONG 1
#define PROC_STATUS_WAIT       2

PLS_PTR_DECLARE(proc_s, __current);
#define current (PLS_PTR(__current))

void proc_switch(proc_t proc);
void proc_wait_pretend(void);
void proc_wait_try(void);

/* ==  SCHEDULER  ============================================= */

void schedule(void);

#endif
