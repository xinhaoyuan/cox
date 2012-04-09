#ifndef __EKOS_LWIP_PERF_H__
#define __EKOS_LWIP_PERF_H__

#define PERF_START // { extern u32_t *hpet_tick, hpet_tick_freq; u32_t __ts = *hpet_tick;
#define PERF_STOP(x) // __ts = ((*hpet_tick - __ts) * 1000000.0 / hpet_tick_freq); if (__ts > 100) ekf_kprintf("PERF %s = %d\n", x, __ts); }

#endif
