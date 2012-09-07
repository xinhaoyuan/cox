#include <types.h>
#include <frame.h>

#include "arch/mem.h"

volatile void *
kmmio_open(uintptr_t addr, size_t size)
{
    return VADDR_DIRECT(addr);
}

void
kmmio_close(volatile void *addr)
{ }

void *
kframe_open(size_t num, unsigned int flags)
{
    void *start = valloc(num);
    if (start == NULL) return NULL;

    /* Touch the map first */
    size_t i;
    for (i = 0; i < num; ++ i)
        if (get_pte(pgdir_scratch, (uintptr_t)start + (i << _MACH_PAGE_SHIFT), flags) == NULL)
        {
            vfree(start);
            return NULL;
        }
    
    if (flags & FA_CONTIGUOUS)
    {
        frame_t head = frame_alloc(num, flags);
        if (head == NULL)
        {
            vfree(start);
            return NULL;
        }

        for (i = 0; i < num; ++ i)
        {
            pte_t *pte = get_pte(pgdir_scratch, (uintptr_t)start + (i << _MACH_PAGE_SHIFT), 0);
            *pte = FRAME_TO_PHYS(head + i) | PTE_W | PTE_P;

            if (i == (num - 1)) *pte |= PTE_KME;
        }
    }
    else
    {
        for (i = 0; i < num; ++ i)
        {
            frame_t frame = frame_alloc(1, flags);
            if (frame == NULL)
            {
                while (1)
                {
                    if (i == 0) break;
                    else --i;
                    
                    pte_t *pte = get_pte(pgdir_scratch, (uintptr_t)start + (i << _MACH_PAGE_SHIFT), 0);
                    frame_put(PHYS_TO_FRAME(PTE_ADDR(*pte)));
                    *pte = 0;
                }
                vfree(start);

                return NULL;
            }
            
            pte_t *pte = get_pte(pgdir_scratch, (uintptr_t)start + (i << _MACH_PAGE_SHIFT), 0);
            *pte = FRAME_TO_PHYS(frame) | PTE_W | PTE_P;

            if (i == (num - 1)) *pte |= PTE_KME;
        }
    }

    return start;
}

void
kframe_close(void *head)
{
    while ((uintptr_t)head >= KVBASE && (uintptr_t)head < KVBASE + KVSIZE)
    {
        pte_t *pte = get_pte(pgdir_scratch, (uintptr_t)head, 0);
        if (pte == NULL) break;
        frame_put(PHYS_TO_FRAME(PTE_ADDR(*pte)));
        int kme = *pte & PTE_KME;
        *pte = 0;

        head += _MACH_PAGE_SIZE;

        if (kme) break;
    }
}
