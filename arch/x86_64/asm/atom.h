#ifndef __ARCH_ATOM_H__
#define __ARCH_ATOM_H__

#include <types.h>

/* Atomic exchange and CAS */

static __inline uint64_t
__xchg64(volatile void *addr, uint64_t newval)
{
     uint32_t result;
  
     // The + in "+m" denotes a read-modify-write operand.
     asm volatile("lock; xchgl %0, %1" :
                  "+m" (*(volatile uint64_t *)addr), "=a" (result) :
                  "1" (newval) :
                  "cc");
     return result;
}

static __inline uint32_t
__xchg32(volatile void *addr, uint32_t newval)
{
     uint32_t result;
  
     // The + in "+m" denotes a read-modify-write operand.
     asm volatile("lock; xchgl %0, %1" :
                  "+m" (*(volatile uint32_t *)addr), "=a" (result) :
                  "1" (newval) :
                  "cc");
     return result;
}

static __inline uint16_t
__xchg16(volatile void *addr, uint16_t newval)
{
     uint16_t result;
  
     // The + in "+m" denotes a read-modify-write operand.
     asm volatile("lock; xchgw %0, %1" :
                  "+m" (*(volatile uint16_t *)addr), "=a" (result) :
                  "1" (newval) :
                  "cc");
     return result;
}

static __inline uint8_t
__xchg8(volatile void *addr, uint8_t newval)
{
     uint8_t result;
  
     // The + in "+m" denotes a read-modify-write operand.
     asm volatile("lock xchgb %0, %1" :
                  "+m" (*(volatile uint8_t *)addr), "=a" (result) :
                  "1" (newval) :
                  "cc");
     return result;
}

static __inline uint64_t
__cmpxchg64(volatile void *addr, uint64_t oldval, uint64_t newval)
{
     uint64_t result;
     // The + in "+m" denotes a read-modify-write operand.
     asm volatile("cmpxchgl %3, %1" :
                  "=a"(result), "+m" (*(volatile uint64_t *)addr) :
                  "a" (oldval), "r" (newval) :
                  "cc");
     return result;
}

static __inline uint32_t
__cmpxchg32(volatile void *addr, uint32_t oldval, uint32_t newval)
{
     uint32_t result;
     // The + in "+m" denotes a read-modify-write operand.
     asm volatile("cmpxchgl %3, %1" :
                  "=a"(result), "+m" (*(volatile uint32_t *)addr) :
                  "a" (oldval), "r" (newval) :
                  "cc");
     return result;
}

static __inline uint16_t
__cmpxchg16(volatile void *addr, uint16_t oldval, uint16_t newval)
{
     uint16_t result;
     // The + in "+m" denotes a read-modify-write operand.
     asm volatile("cmpxchgw %3, %1" :
                  "=a"(result), "+m" (*(volatile uint16_t *)addr) :
                  "a" (oldval), "r" (newval) :
                  "cc");
     return result;
}

static __inline uint8_t
__cmpxchg8(volatile void *addr, uint8_t oldval, uint8_t newval)
{
     uint8_t result;
     // The + in "+m" denotes a read-modify-write operand.
     asm volatile("cmpxchgb %3, %1" :
                  "=a"(result), "+m" (*(volatile uint8_t *)addr) :
                  "a" (oldval), "r" (newval) :
                  "cc");
     return result;
}

#endif
