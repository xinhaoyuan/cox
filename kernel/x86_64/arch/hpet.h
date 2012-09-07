#ifndef __KERN_ARCH_HPET_H__
#define __KERN_ARCH_HPET_H__

#include <types.h>

extern volatile const uint64_t *hpet_tick;
extern uint64_t                 hpet_tick_freq;

int hpet_init(void);

#endif
