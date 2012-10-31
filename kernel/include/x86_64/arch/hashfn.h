#ifndef __KERN_ARCH_HASHFN_H__
#define __KERN_ARCH_HASHFN_H__

#include <types.h>

static inline __attribute__((always_inline)) uintptr_t
hash_ptr(uintptr_t key)
{
    /* simplified FNV-1a hash */
    return (key ^ 14695981039346656037ULL) * 1099511628211;
}

#endif
