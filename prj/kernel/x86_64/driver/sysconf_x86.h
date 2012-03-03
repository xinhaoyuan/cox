#ifndef __KERN_ARCH_SYSCONF_X86_H__
#define __KERN_ARCH_SYSCONF_X86_H__

#include <types.h>

/* Define the structure for configuration of arch  */

typedef struct sysconf_x86_s
{
	struct {
		int boot;
		int count;
	} cpu;

	struct {
		bool      enable;
		uintptr_t phys;
	} lapic;

	struct
	{
		bool enable;
		int  count;
		bool use_eoi;
	} ioapic;

	struct
	{
		bool      enable;
		uintptr_t phys;
	} hpet;
	
} sysconf_x86_s;

extern sysconf_x86_s sysconf_x86;

#endif
