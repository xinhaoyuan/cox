#define DEBUG_COMPONENT DBG_MEM

#include <arch/memlayout.h>
#include <debug.h>

#include "memmap.h"
#include "boot_alloc.h"

/* Boot alloc is a naive allocator that get free memory directly after
 * the kernel data. There are symbol provided in the link-script which
 * indicate the end address of kernel, then the allocator would follow
 * the address to allocate memory blocks */

static uintptr_t boot_alloc_start;
static uintptr_t boot_free;
static int       bf_memmap;

extern char __end[];

void
boot_alloc_init(void)
{
    bf_memmap = 0;
    boot_alloc_start = 
        boot_free = (uintptr_t)__end;

    uintptr_t start, end;
    while (memmap_enumerate(bf_memmap, &start, &end) == 0)
    {
        if (boot_free < end) break;
        ++ bf_memmap;
    }

    if (memmap_enumerate(bf_memmap, &start, &end) != 0)
    {
        PANIC("cannot find memory area for boot alloc\n");
    }
}

void
boot_alloc_get_area(uintptr_t *start, uintptr_t *end)
{
    if (start) *start = boot_alloc_start;
    if (end)   *end   = boot_free;
}

void *
boot_alloc(uintptr_t size, uintptr_t align, int verbose)
{
     if (verbose)
          DEBUG("[boot_alloc] boot_free = %p, size = 0x%016lx, align = 0x%x\n", boot_free, size, align);

     uintptr_t start, end;
     while (1)
     {
         if (memmap_enumerate(bf_memmap, &start, &end) != 0)
             return NULL;

         if (boot_free < start) boot_free = start;

         /* Find the continuous memory space that would contain the
          * need */
         if ((boot_free |=  align - 1) + size < end)
             break;
         else ++ bf_memmap;
     }

     ++ boot_free;
     void *result = VADDR_DIRECT(boot_free);
     
     if (verbose)
          DEBUG("[boot_alloc] result = %p\n", result);

     boot_free += size;
     return result;
}
