#ifndef __KERN_TIMER_H__
#define __KERN_TIMER_H__

#include <types.h>
#include <spinlock.h>
#include <lib/crh.h>

typedef crh_key_t tick_t;
extern  tick_t    timer_tick;

struct timer_s
{
    int in;
    int type;
    crh_node_s node;

    union
    {
        struct proc_s *wakeup_proc;
    };
};

#define TIMER_TYPE_WAKEUP 0

typedef struct timer_s timer_s;
typedef timer_s *timer_t;

void   timer_master_cpu_set(void);
int    timer_sys_init(void);
int    timer_sys_init_mp(void);

tick_t timer_tick_get(void);

void   timer_init(timer_t timer, int type);
int    timer_set(timer_t timer, tick_t tick);

#endif
