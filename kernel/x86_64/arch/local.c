#define DEBUG_COMPONENT DBG_MEM

#include <asm/mach.h>
#include <asm/cpu.h>
#include <asm/atom.h>
#include <asm/mmu.h>
#include <string.h>
#include <frame.h>
#include <arch/memlayout.h>
#include <arch/local.h>
#include <error.h>

#include "sysconf_x86.h"
#include "mem.h"
#include "lapic.h"

extern char pls_start[];
extern char pls_end[];

PLS_ATOM_DEFINE(uintptr_t, __pls_base, 0);

volatile static unsigned int pls_init_barrier = 0;

int
pls_init(void)
{
    size_t pls_pages = (pls_end - pls_start + _MACH_PAGE_SIZE - 1) >> _MACH_PAGE_SHIFT;
    void  *pls_area;

    if (lapic_id() == sysconf_x86.cpu.boot)
    {
        while (pls_init_barrier != sysconf_x86.cpu.count - 1) __cpu_relax();

        pls_area = pls_start;
    }
    else
    {
        pls_area = frame_arch_kopen(pls_pages);
        
        if (pls_area == NULL) return -E_NO_MEM;
        
        /* copy the initial data */
        memmove(pls_area, pls_start, pls_end - pls_start);

        while (1)
        {
            unsigned int old = pls_init_barrier;
            if (__cmpxchg32(&pls_init_barrier, old, old + 1) == old)
                break;
            __cpu_relax();
        }
    }
    
    uintptr_t base = (uintptr_t)pls_area - (uintptr_t)pls_start;

    /* set and load segment */
    /* also init gs here for userspace TLS */
    gdt[SEG_PLS(lapic_id())] = SEG(STA_W, DPL_KERNEL | 3);
    gdt[SEG_TLS(lapic_id())] = SEG(STA_W, DPL_KERNEL | 3);
    __asm__ __volatile__("movw %w0, %%fs" : : "a"(GD_PLS(lapic_id()) | 3));
    __asm__ __volatile__("movw %w0, %%gs" : : "a"(GD_TLS(lapic_id()) | 3));
    /* write the 64-bit fs segment base by msr */      
    __write_msr(0xC0000100, base);

    PLS_SET(__pls_base, base);

    /* Simple test */
    // DEBUG("__pls_base = %x\n", PLS(__pls_base));
    uintptr_t test = 0x1234;
    __asm__ __volatile__("mov %%fs:(%1), %0": "=r"(test) : "r"((uintptr_t)&base - base));
    if (base != test)
    {
        PANIC("PANIC: processor local storage failed to initailize.", base, test);
    }

    return 0;
}
