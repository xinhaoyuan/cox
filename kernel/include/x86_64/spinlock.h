#ifndef __KERN_ARCH_SPINLOCK_H__
#define __KERN_ARCH_SPINLOCK_H__

#include <types.h>
#include <asm/atom.h>

typedef volatile uint32_t spinlock_s;
typedef spinlock_s *spinlock_t;

#define SPINLOCK_UNLOCKED_INITIALIZER {0}

static __inline void spinlock_init(spinlock_t lock) __attribute__((always_inline));
static __inline bool spinlock_try_acquire(spinlock_t lock) __attribute__((always_inline)); /* 0 for failed */
static __inline void spinlock_acquire(spinlock_t lock) __attribute__((always_inline));
static __inline void spinlock_release(spinlock_t lock) __attribute__((always_inline));

static __inline void
spinlock_init(spinlock_t lock)
{
    *lock = 0;
}

static __inline bool
spinlock_try_acquire(spinlock_t lock)
{
    return !__xchg32(lock, 1);
}

static __inline void
spinlock_acquire(spinlock_t lock)
{
    while (__xchg32(lock, 1) == 1) ;
}

static __inline void
spinlock_release(spinlock_t lock)
{
    // *lock = 0;
    __xchg32(lock, 0);
}

#endif
