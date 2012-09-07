#ifndef __ARCH_ASM_CPU_H__
#define __ARCH_ASM_CPU_H__

#include <types.h>

/* Pseudo-descriptors used for LGDT, LLDT(not used) and LIDT instructions. */
struct pseudodesc {
    uint16_t pd_lim;        // Limit
    uintptr_t pd_base;      // Base address
} __attribute__ ((packed));

static inline void __cpu_relax(void) __attribute__((always_inline));
static inline void __lidt(struct pseudodesc *pd) __attribute__((always_inline));
static inline void __sti(void) __attribute__((always_inline));
static inline void __cli(void) __attribute__((always_inline));
static inline void __ltr(uint16_t sel) __attribute__((always_inline));
static inline void __lcr0(uintptr_t cr0) __attribute__((always_inline));
static inline void __lcr3(uintptr_t cr3) __attribute__((always_inline));
static inline uintptr_t __rcr0(void) __attribute__((always_inline));
static inline uintptr_t __rcr1(void) __attribute__((always_inline));
static inline uintptr_t __rcr2(void) __attribute__((always_inline));
static inline uintptr_t __rcr3(void) __attribute__((always_inline));
static inline void __invlpg(void *addr) __attribute__((always_inline));
static inline uint64_t __read_rbp(void) __attribute__((always_inline));
static inline uint64_t __read_rflags(void) __attribute__((always_inline));
static inline void __write_rflags(uint64_t rflags) __attribute__((always_inline));
static inline void __write_msr(uint32_t idx, uint64_t value) __attribute__((always_inline));

static inline void
__cpu_relax(void)
{
	__asm__ __volatile__ ("pause");
}

static inline void
__lidt(struct pseudodesc *pd) {
    __asm__ __volatile__ ("lidt (%0)" :: "r" (pd) : "memory");
}

static inline void
__sti(void) {
    __asm__ __volatile__ ("sti");
}

static inline void
__cli(void) {
    __asm__ __volatile__ ("cli" ::: "memory");
}

static inline void
__ltr(uint16_t sel) {
    __asm__ __volatile__ ("ltr %0" :: "a" (sel) : "memory");
}

static inline void
__lcr0(uintptr_t cr0) {
    __asm__ __volatile__ ("mov %0, %%cr0" :: "r" (cr0) : "memory");
}

static inline void
__lcr3(uintptr_t cr3) {
    __asm__ __volatile__ ("mov %0, %%cr3" :: "r" (cr3) : "memory");
}

static inline uintptr_t
__rcr0(void) {
    uintptr_t cr0;
    __asm__ __volatile__ ("mov %%cr0, %0" : "=r" (cr0) :: "memory");
    return cr0;
}

static inline uintptr_t
__rcr1(void) {
    uintptr_t cr1;
    asm volatile ("mov %%cr1, %0" : "=r" (cr1) :: "memory");
    return cr1;
}

static inline uintptr_t
__rcr2(void) {
    uintptr_t cr2;
    asm volatile ("mov %%cr2, %0" : "=r" (cr2) :: "memory");
    return cr2;
}

static inline uintptr_t
__rcr3(void) {
    uintptr_t cr3;
    __asm__ __volatile__ ("mov %%cr3, %0" : "=r" (cr3) :: "memory");
    return cr3;
}

static inline void
__invlpg(void *addr) {
    __asm__ __volatile__ ("invlpg (%0)" :: "r" (addr) : "memory");
}

static inline uint64_t
__read_rbp(void) {
    uint64_t rbp;
    __asm__ __volatile__ ("movq %%rbp, %0" : "=r" (rbp));
    return rbp;
}

static inline uint64_t
__read_rflags(void) {
    uint64_t rflags;
    __asm__ __volatile__ ("pushfq; popq %0" : "=r" (rflags));
    return rflags;
}

static inline void
__write_rflags(uint64_t rflags) {
    __asm__ __volatile__ ("pushq %0; popfq" :: "r" (rflags));
}

static inline void
__cpuid(uint32_t info, uint32_t *eaxp, uint32_t *ebxp, uint32_t *ecxp, uint32_t *edxp)
{
     uint32_t eax, ebx, ecx, edx;
     asm volatile("cpuid" 
		  : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
		  : "a" (info));
     if (eaxp)
	  *eaxp = eax;
     if (ebxp)
	  *ebxp = ebx;
     if (ecxp)
	  *ecxp = ecx;
     if (edxp)
	  *edxp = edx;
}


static inline void
__write_msr(uint32_t idx, uint64_t value)
{
	asm volatile("wrmsr"
				 : : "a" (value & 0xFFFFFFFF), "c" (idx), "d" ((value >> 32) & 0xFFFFFFFF));
}

#endif
