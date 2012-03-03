#ifndef __ARCH_ASM_BIT_H__
#define __ARCH_ASM_BIT_H__

#include <types.h>

static inline int __bsf(uint64_t n) __attribute__((always_inline));
static inline int __bsr(uint64_t n) __attribute__((always_inline));

static inline int
__bsf(uint64_t n)
{
	if (n == 0) return -1;
	uint64_t result;
	__asm __volatile("bsfq %1, %0"
					 : "=r" (result)
					 : "r" (n));
	return result;
}

static inline int
__bsr(uint64_t n)
{
	if (n == 0) return -1;
	uint64_t result;
	__asm __volatile("bsrq %1, %0"
					 : "=r" (result)
					 : "r" (n));
	return result;
}

#define BIT_SEARCH_FIRST __bsf
#define BIT_SEARCH_LAST  __bsr

#endif
