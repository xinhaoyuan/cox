#ifndef __KERN_ARCH_LAPIC_H__
#define __KERN_ARCH_LAPIC_H__

/* Max number of logical cpu (and LAPICs) can be supported */
#define LAPIC_COUNT 256

#ifndef __ASSEMBLER__

#include <types.h>

int  lapic_init(void);
int  lapic_init_ap(void);
int  lapic_id();
void lapic_eoi_send(void);
void lapic_ap_start(int apicid, uint32_t addr);
void lapic_timer_set(uint32_t freq);
int  lapic_ipi_issue(int lapic_id);
int  lapic_ipi_issue_spec(int lapic_id, uint8_t vector);

#endif

#endif
