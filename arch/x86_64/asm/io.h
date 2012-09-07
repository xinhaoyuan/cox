#ifndef __ARCH_ASM_IO_H__
#define __ARCH_ASM_IO_H__

#include <types.h>

#define __barrier() __asm__ __volatile__ ("" ::: "memory")

static inline uint8_t  __inb(uint16_t port) __attribute__((always_inline));
static inline uint16_t __inw(uint16_t port) __attribute__((always_inline));
static inline uint32_t __inl(uint16_t port) __attribute__((always_inline));
static inline void     __insl(uint32_t port, void *addr, int cnt) __attribute__((always_inline));
static inline void     __outb(uint16_t port, uint8_t data) __attribute__((always_inline));
static inline void     __outw(uint16_t port, uint16_t data) __attribute__((always_inline));
static inline void     __outl(uint16_t port, uint32_t data) __attribute__((always_inline));
static inline void     __outsl(uint32_t port, const void *addr, int cnt) __attribute__((always_inline));

static inline uint8_t
__inb(uint16_t port) {
    uint8_t data;
    asm volatile ("inb %1, %0" : "=a" (data) : "d" (port) : "memory");
    return data;
}


static inline uint16_t
__inw(uint16_t port) {
    uint16_t data;
    asm volatile ("inw %1, %0" : "=a" (data) : "d" (port) : "memory");
    return data;
}


static inline uint32_t
__inl(uint16_t port) {
    uint32_t data;
    asm volatile ("inl %1, %0" : "=a" (data) : "d" (port) : "memory");
    return data;
}

static inline void
__insl(uint32_t port, void *addr, int cnt) {
    asm volatile (
        "cld;"
        "repne; insl;"
        : "=D" (addr), "=c" (cnt)
        : "d" (port), "0" (addr), "1" (cnt)
        : "memory", "cc");
}

static inline void
__outb(uint16_t port, uint8_t data) {
    asm volatile ("outb %0, %1" :: "a" (data), "d" (port) : "memory");
}

static inline void
__outw(uint16_t port, uint16_t data) {
    asm volatile ("outw %0, %1" :: "a" (data), "d" (port) : "memory");
}

static inline void
__outl(uint16_t port, uint32_t data) {
    asm volatile ("outl %0, %1" :: "a" (data), "d" (port) : "memory");
}

static inline void
__outsl(uint32_t port, const void *addr, int cnt) {
    asm volatile (
        "cld;"
        "repne; outsl;"
        : "=S" (addr), "=c" (cnt)
        : "d" (port), "0" (addr), "1" (cnt)
        : "memory", "cc");
}

#endif
