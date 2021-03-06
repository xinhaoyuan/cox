#define DEBUG_COMPONENT DBG_IO

#include <frame.h>
#include <debug.h>

#include "mp.h"
#include "sysconf_x86.h"
#include "mem.h"
#include "hpet.h"

volatile uint64_t       *hpet_regs;
volatile const uint64_t *hpet_tick;
uint64_t                 hpet_tick_freq;

static int      hpet_timer_count;
static uint32_t hpet_period;

#define HR_GEN_CAP_ID 0
#define HR_GEN_CONF   2
#define HR_IRQ_STATUS 4
#define HR_MAIN_COUNTER_VALUE 30

#define HRT_BASE(n)    ((n << 2) + 32)
#define HRT_CONF(n)    (HRT_BASE(n) + 0)
#define HRT_COMP(n)    (HRT_BASE(n) + 1)
#define HRT_FSB_INT(n) (HRT_BASE(n) + 2)

static void __attribute__((unused))
hpet_enable_timer(int t) 
{
    uint64_t id = hpet_regs[HRT_CONF(t)];
    /* disable fsb */
    id &= ~((uint64_t)1 << 14);
    /* force 32-bit mode */
    if (id & ((uint64_t)1 << 4))
        id &= ~((uint64_t)1 << 3);
    /* enable the interrupt 2, edge triggered */
    id &= ~((uint64_t)0x1f << 9);
    id |=  ((uint64_t)2 << 9);
        
    id |= ((uint64_t)1 << 2);
    id &= ~((uint64_t)1 << 1);

    hpet_regs[HRT_CONF(t)] = id;
}

int
hpet_init(void)
{
    if (!sysconf_x86.hpet.enable) return 0;

    
    hpet_regs = mmio_arch_kopen(sysconf_x86.hpet.phys, PGSIZE);
    
    uint32_t id_hi = hpet_regs[HR_GEN_CAP_ID] >> 32;
    uint32_t id_lo = hpet_regs[HR_GEN_CAP_ID];
    
#if 0
    DEBUG("HPET INFO[%08x %08x]:\n    rev_id 0x%02x hpet_timers: %d width: %d period: %d (10^-15 s)\n",
            id_hi, id_lo,
            id_lo & 0xff, (id_lo >> 8) & 0xf, (id_lo & (1 << 13)) ? 64 : 32, id_hi);
#endif

    // hpet_counter_width = (id_lo & (1 << 13)) ? 64 : 32;
    hpet_period = id_hi;
    hpet_timer_count = (id_lo >> 8) & 0xf;
    
    /* Set the configuration to enable CNF and disable legacy replacement */
    hpet_regs[HR_GEN_CONF] = 1;
    hpet_tick = (volatile uint64_t *)(hpet_regs + HR_MAIN_COUNTER_VALUE);
    
    hpet_tick_freq = 1000000000000000.0 / hpet_period;
    DEBUG("HPET FREQ = %d\n", hpet_tick_freq);
    
    return 0;
}
