#ifndef __KERN_ARCH_IRQ_H__
#define __KERN_ARCH_IRQ_H__

#include <cpu.h>
#include <mmu.h>

#define IRQ_COUNT               32

#define IRQ_SCTL                2

#define IRQ_TIMER               0
#define IRQ_KBD                 1
#define IRQ_COM1                4
#define IRQ_IDE1                14
#define IRQ_IDE2                15
#define IRQ_ERROR               19
#define IRQ_SPURIOUS            31

#define __irq_enable()  __sti()
#define __irq_disable() __cli()

static inline int
__irq_save(void) {
    if (__read_rflags() & FL_IF) {
        __irq_disable();
        return 1;
    }
    return 0;
}

static inline void
__irq_restore(int flag) {
    if (flag) {
        __irq_enable();
    }
}
#endif
