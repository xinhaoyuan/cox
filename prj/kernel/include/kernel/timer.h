#ifndef __KERN_TIMER_H__
#define __KERN_TIMER_H__

#include <types.h>
#include <spinlock.h>
#include <lib/crh.h>

typedef crh_key_t tick_t;
extern tick_t     timer_tick;

struct proc_timer_s
{
    int in;
    crh_node_s node;
};

typedef struct proc_timer_s proc_timer_s;
typedef proc_timer_s *proc_timer_t;

void timer_master_cpu_set(void);
int  timer_init(void);
int  timer_init_mp(void);

tick_t timer_tick_get(void);

void proc_timer_init(proc_timer_t timer);
int  proc_timer_set(proc_timer_t timer, tick_t tick);

#endif
