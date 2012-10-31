#include <frame.h>
#include "arch/mem.h"

volatile void *
mmio_arch_kopen(uintptr_t addr, size_t size)
{
    return VADDR_DIRECT(addr);
}

void
mmio_arch_kclose(volatile void *addr)
{ }

void *
frame_arch_kopen(size_t num)
{
    void *start = valloc(num);
    if (start == NULL) return NULL;

    /* Touch the address space */
    size_t i;
    for (i = 0; i < num; ++ i)
        if (get_pte(pgdir_kernel, (uintptr_t)start + (i << _MACH_PAGE_SHIFT), 1) == NULL)
        {
            vfree(start);
            return NULL;
        }

    for (i = 0; i < num; ++ i)
    {
        frame_t frame = frame_alloc(1);
        if (frame == NULL)
        {
            /* If failed, clear all frame allocated. But may remain
             * frames for page-table */
            while (1)
            {
                if (i == 0) break;
                else --i;
                    
                pte_t *pte = get_pte(pgdir_kernel, (uintptr_t)start + (i << _MACH_PAGE_SHIFT), 0);
                frame_put(PHYS_TO_FRAME(PTE_ADDR(*pte)));
                *pte = 0;
            }
                
            vfree(start);
            return NULL;
        }
            
        pte_t *pte = get_pte(pgdir_kernel, (uintptr_t)start + (i << _MACH_PAGE_SHIFT), 0);
        *pte = FRAME_TO_PHYS(frame) | PTE_W | PTE_P;

        if (i == (num - 1)) *pte |= PTE_KME;
    }

    return start;
}

void
frame_arch_kclose(void *head)
{
    while ((uintptr_t)head >= KVBASE && (uintptr_t)head < KVBASE + KVSIZE)
    {
        pte_t *pte = get_pte(pgdir_kernel, (uintptr_t)head, 0);
        if (!pte) break;
        
        frame_t cur = PHYS_TO_FRAME(PTE_ADDR(*pte));
        
        int kme = *pte & PTE_KME;
        *pte = 0;

        /* Only put head frames */
        if ((cur->region & 1) == 0) frame_put(cur);
        head += _MACH_PAGE_SIZE;
        if (kme) return;
    }

    PANIC("Invalid frame area for frame_arch_kclose");
}
