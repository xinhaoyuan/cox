#define DEBUG_COMPONENT DBG_MEM

#include <types.h>
#include <error.h>

#include <arch/memlayout.h>
#include <debug.h>

#include "memmap.h"
#include "init.h"
#include "e820.h"

#define E820MAX             20      // number of entries in E820MAP
#define E820_ARM            1       // address range memory
#define E820_ARR            2       // address range reserved

typedef struct multiboot_memory_map_s {
     uint32_t size;
     uint64_t base;
     uint64_t length;
     uint32_t type;
} __attribute__((packed)) *multiboot_memory_map_t;

struct __e820map {
    int nr_map;
    struct {
        uint64_t addr;
        uint64_t size;
        uint32_t type;
    } __attribute__((packed)) map[E820MAX];
};

int
memmap_process_e820(void)
{
    memmap_reset();

    if (mb_magic == 0x2BADB002)
    {
        /* e820 from GRUB */
        
        uint32_t *_mb_info  = (uint32_t *)VADDR_DIRECT(mb_info_phys);
        if ((_mb_info[0] & (1 << 6)) == 0)
        {
            /* What should I do? */
            PANIC("No MMAP from grub\n");
        }
        else
        {
            char    *_mmap_addr = (char *)VADDR_DIRECT(_mb_info[12]);
            uint32_t _mmap_size = _mb_info[11];
            char    *_mmap_end = _mmap_addr + _mmap_size;
            while (_mmap_addr < _mmap_end)
            {
                multiboot_memory_map_t mb_mmap = (multiboot_memory_map_t)_mmap_addr;
                memmap_append(mb_mmap->base, mb_mmap->base + mb_mmap->length, mb_mmap->type);
                _mmap_addr += mb_mmap->size + sizeof(mb_mmap->size);
            }
        }
    }
    else
    {
        /* e820 from native bootloader */
        
        struct __e820map *memmap = (struct __e820map *)(0x8000 + PHYSBASE);
        uintptr_t maxpa = 0;
        
        int i;
        for (i = 0; i < memmap->nr_map; i ++) {
            uint64_t begin = memmap->map[i].addr, end = begin + memmap->map[i].size;
            memmap_append(begin, end, memmap->map[i].type);
            
            if (memmap->map[i].type == E820_ARM) {
                if (maxpa < end && begin < PHYSSIZE) {
                    maxpa = end;
                }
            }
        }
    }

    memmap_process(1);
    return 0;
}
