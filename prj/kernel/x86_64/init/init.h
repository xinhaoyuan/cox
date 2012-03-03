#ifndef __KERN_ARCH_INIT_H__
#define __KERN_ARCH_INIT_H__

extern uint32_t mb_magic;
extern uint32_t mb_info_phys;

void __kern_early_init(void) __attribute__((noreturn));
void __kern_cpu_init(void) __attribute__((noreturn));

#endif
