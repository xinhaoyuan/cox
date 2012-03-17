#ifndef __SYNC_H__
#define __SYNC_H__

#include <types.h>
#include <spinlock.h>

/* Assume that the pointer is 4-aligned */
/* NEAR-STANDARD SYNC INTERFACE */
struct ips_node_s;
typedef volatile struct ips_node_s ips_node_s;
typedef ips_node_s *ips_node_t;

#define IPS_NODE_WAIT(node)    ((node)->data & 1)
#define IPS_NODE_AC_WAIT(node) ((node)->data & 2)
#define IPS_NODE_PTR(node)     ((void *)((node)->data & ~(uintptr_t)3))

#define IPS_NODE_WAIT_SET(node)      ((node)->data |= 1)
#define IPS_NODE_WAIT_CLEAR(node)    ((node)->data &= ~(uintptr_t)1)
#define IPS_NODE_AC_WAIT_SET(node)   ((node)->data |= 2)
#define IPS_NODE_AC_WAIT_CLEAR(node) ((node)->data &= ~(uintptr_t)2)
#define IPS_NODE_PTR_SET(node, ptr)  ((node)->data = ((node)->data & 3) | ((uintptr_t)(ptr) & ~(uintptr_t)3))

struct ips_node_s
{
	uintptr_t  data;
	ips_node_t prev, next;
};

#define MUTEX_HOLD(mutex) ((mutex)->data & 1)
#define MUTEX_WAIT(mutex) ((mutex)->data & 2)
#define MUTEX_PTR(mutex)  ((void *)((mutex)->data & ~(uintptr_t)3))

#define MUTEX_HOLD_SET(mutex)     ((mutex)->data |= 1)
#define MUTEX_HOLD_CLEAR(mutex)   ((mutex)->data &= ~(uintptr_t)1)
#define MUTEX_WAIT_SET(mutex)     ((mutex)->data |= 2)
#define MUTEX_WAIT_CLEAR(mutex)   ((mutex)->data &= ~(uintptr_t)2)
#define MUTEX_PTR_SET(mutex, ptr) ((mutex)->data = ((mutex)->data & 3) | ((uintptr_t)(ptr) & ~(uintptr_t)3))

struct mutex_s
{
	spinlock_s lock;
	uintptr_t  data;
};

typedef volatile struct mutex_s mutex_s;
typedef mutex_s *mutex_t;

#define SEMAPHORE_WAIT(sem) ((sem)->data & 2)
#define SEMAPHORE_PTR(sem)  ((void *)((sem)->data & ~(uintptr_t)3))

#define SEMAPHORE_WAIT_SET(sem)     ((sem)->data |= 2)
#define SEMAPHORE_WAIT_CLEAR(sem)   ((sem)->data &= ~(uintptr_t)2)
#define SEMAPHORE_PTR_SET(sem, ptr) ((sem)->data = ((sem)->data & 3) | ((uintptr_t)(ptr) & ~(uintptr_t)3))

struct semaphore_s
{
	spinlock_s lock;
	uintptr_t  count;
	uintptr_t  data;
};

typedef volatile struct semaphore_s semaphore_s;
typedef semaphore_s *semaphore_t;

int  ips_is_finish(ips_node_t node);
void ips_wait(ips_node_t node);
/* 0 for break/success, 1 for wait(again) */
int  ips_wait_try(ips_node_t node);

void mutex_init(mutex_t mutex);
int  mutex_try_acquire(mutex_t mutex);
int  mutex_acquire(mutex_t mutex, ips_node_t node);
void mutex_ac_break(mutex_t mutex, ips_node_t node);
void mutex_release(mutex_t mutex);

void semaphore_init(semaphore_t sem, uintptr_t count);
int  semaphore_try_acquire(semaphore_t sem);
int  semaphore_acquire(semaphore_t sem, ips_node_t node);
void semaphore_ac_break(semaphore_t sem, ips_node_t node);
/* 1 for notify, 0 for no notify */
int  semaphore_release(semaphore_t sem);

#endif
