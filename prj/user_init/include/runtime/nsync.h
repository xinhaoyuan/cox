#ifndef __NSYNC_H__
#define __NSYNC_H__

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

#define NATIVE_SEM_WAIT(sem) ((sem)->data & 2)
#define NATIVE_SEM_PTR(sem)  ((void *)((sem)->data & ~(uintptr_t)3))

#define NATIVE_SEM_WAIT_SET(sem)     ((sem)->data |= 2)
#define NATIVE_SEM_WAIT_CLEAR(sem)   ((sem)->data &= ~(uintptr_t)2)
#define NATIVE_SEM_PTR_SET(sem, ptr) ((sem)->data = ((sem)->data & 3) | ((uintptr_t)(ptr) & ~(uintptr_t)3))

struct native_sem_s
{
    spinlock_s lock;
    uintptr_t  count;
    uintptr_t  data;
};

typedef volatile struct native_sem_s native_sem_s;
typedef native_sem_s *native_sem_t;

int  ips_is_finish(ips_node_t node);
void ips_wait(ips_node_t node);
/* 1 for break/success, 0 for wait(again) */
int  ips_wait_try(ips_node_t node);

void native_sem_init(native_sem_t sem, uintptr_t count);
/* returns count before acquire */
uintptr_t native_sem_try_acquire(native_sem_t sem);
uintptr_t native_sem_acquire(native_sem_t sem, ips_node_t node);
void native_sem_ac_break(native_sem_t sem, ips_node_t node);
int  native_sem_release(native_sem_t sem);

#endif
