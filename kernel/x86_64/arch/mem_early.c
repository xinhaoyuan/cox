#define DEBUG_COMPONENT DBG_MEM

#include <string.h>
#include <debug.h>
#include <spinlock.h>
#include <frame.h>

#include "mem_early.h"
#include "mem.h"
#include "memmap.h"
#include "e820.h"
#include "boot_alloc.h"

uintptr_t kern_start;
uintptr_t kern_end;
uintptr_t phys_end;
uintptr_t nr_pages;

/* virtual address of boot-time page directory */
static pgd_t *boot_pgdir = NULL;
/* physical address of boot-time page directory */
static uintptr_t boot_cr3;

static struct taskstate ts_boot = {0};

static pgd_t *
boot_get_pgd(pgd_t *pgdir, uintptr_t la, int create) {
    return &pgdir[PGX(la)];
}

static pud_t *
boot_get_pud(pgd_t *pgdir, uintptr_t la, int create) {
    pgd_t *pgdp;
    if ((pgdp = boot_get_pgd(pgdir, la, create)) == NULL) {
        return NULL;
    }
    if (!(*pgdp & PTE_P)) {
        pud_t *pud;
        if (!create || (pud = boot_alloc(PGSIZE, PGSIZE, 0)) == NULL) {
            return NULL;
        }
        uintptr_t pa = PADDR_DIRECT(pud);
        memset(pud, 0, PGSIZE);
        *pgdp = pa | PTE_U | PTE_W | PTE_P;
    }
    return &((pud_t *)VADDR_DIRECT(PGD_ADDR(*pgdp)))[PUX(la)];
}

static pmd_t *
boot_get_pmd(pgd_t *pgdir, uintptr_t la, bool create) {
    pud_t *pudp;
    if ((pudp = boot_get_pud(pgdir, la, create)) == NULL) {
        return NULL;
    }
    if (!(*pudp & PTE_P)) {
        pmd_t *pmd;
        if (!create || (pmd = boot_alloc(PGSIZE, PGSIZE, 0)) == NULL) {
            return NULL;
        }
        uintptr_t pa = PADDR_DIRECT(pmd);
        memset(pmd, 0, PGSIZE);
        *pudp = pa | PTE_U | PTE_W | PTE_P;
    }
    return &((pmd_t *)VADDR_DIRECT(PUD_ADDR(*pudp)))[PMX(la)];
}

static pte_t *
boot_get_pte(pgd_t *pgdir, uintptr_t la, int create) {
    pmd_t *pmdp;
    if ((pmdp = boot_get_pmd(pgdir, la, create)) == NULL) {
        return NULL;
    }
    
    if (!(*pmdp & PTE_P)) {
        pte_t *pte;
        if (!create || (pte = boot_alloc(PGSIZE, PGSIZE, 0)) == NULL) {
            return NULL;
        }
        uintptr_t pa = PADDR_DIRECT(pte);
        memset(pte, 0, PGSIZE);
        *pmdp = pa | PTE_U | PTE_W | PTE_P;
    }

    return &((pte_t *)VADDR_DIRECT(PMD_ADDR(*pmdp)))[PTX(la)];
}

static void
boot_map_segment(pgd_t *pgdir, uintptr_t la, size_t size, uintptr_t pa, uintptr_t perm) {
    assert(PGOFF(la) == PGOFF(pa));
    size_t n = (size + PGOFF(la) + PGSIZE - 1) / PGSIZE;
    la = la & ~(uintptr_t)(PGSIZE - 1);
    pa = pa & ~(uintptr_t)(PGSIZE - 1);
    for (; n > 0; n --, la += PGSIZE, pa += PGSIZE) {
        pte_t *ptep = boot_get_pte(pgdir, la, 1);
        assert(ptep != NULL);
        *ptep = pa | PTE_P | perm;
    }
}

static void
mmu_init(void)
{
    int i;
        
    pgdir_kernel = boot_pgdir = boot_alloc(PGSIZE, PGSIZE, 0);
    memset(boot_pgdir, 0, PGSIZE);
    boot_cr3 = PADDR_DIRECT(boot_pgdir);

    // recursively insert boot_pgdir in itself
    // to form a virtual page table at virtual address VPT
    boot_pgdir[PGX(VPT)] = PADDR_DIRECT(boot_pgdir) | PTE_P | PTE_W;

    int id = 0;
    uintptr_t start, end;
    while (memmap_enumerate(id, &start, &end) == 0) ++ id;
    phys_end = end;
    nr_pages = (phys_end + PGSIZE - 1) >> PGSHIFT;

    DEBUG("phys max boundary: %p\n", phys_end);
    DEBUG("number of pages: %d\n", nr_pages);

    /* touch all pud page for PHYSBASE ~ PHYSBASE + PHYSSIZE, so all
     * page tables shares the same mmio mapping*/
    int num_of_pudtables = (PHYSSIZE + PUSIZE - 1) / PUSIZE;
    for (i = 0; i < num_of_pudtables; ++ i)
    {
        boot_get_pud(boot_pgdir, PHYSBASE + PUSIZE * i, 1);
    }
    
    /* same for kv mapping */
    num_of_pudtables = (KVSIZE + PUSIZE - 1) / PUSIZE;
    for (i = 0; i < num_of_pudtables; ++ i)
    {
        boot_get_pud(boot_pgdir, KVBASE + PUSIZE * i, 1);
    }
    
    /* map all physical memory at PHYSBASE by 2m pages*/
    int num_of_2m_pages = (phys_end + 0x1fffff) / 0x200000;
    for (i = 0; i < num_of_2m_pages; ++ i)
    {
        uintptr_t phys = i << 21;
        pmd_t *pmd = boot_get_pmd(boot_pgdir, PHYSBASE + phys, 1);
        *pmd = phys | PTE_PS | PTE_W | PTE_P;
    }

    /* and map the first 1MB for the application processor booting */
    boot_map_segment(boot_pgdir, 0, 0x100000, 0, PTE_W);
    
    extern char __text[], __end[];
    kern_start = (uintptr_t)__text;
    kern_end   = (uintptr_t)__end;
    DEBUG("kernel boundary: %p - %p\n", kern_start, kern_end);
    /* remap kernel self */
    boot_map_segment(boot_pgdir, KERNBASE + kern_start, kern_end - kern_start, kern_start, PTE_W);

    __lcr3(boot_cr3);

    // set CR0
    uint64_t cr0 = __rcr0();
    cr0 |= CR0_PE | CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP;
    cr0 &= ~(CR0_TS | CR0_EM);
    __lcr0(cr0);

    memset(&ts_boot, sizeof(ts_boot), 0);
    // initialize the BOOT TSS
    gdt[SEG_TSS_BOOT] = SEGTSS(STS_T32A, (uintptr_t)&ts_boot, sizeof(ts_boot), DPL_KERNEL);
    // then for mp
    for (i = SEG_PER_CPU_START; i < SEG_COUNT; ++ i)
    {
        gdt[i] = SEG_NULL;
    }

    // reload all segment registers
    lgdt(&gdt_pd);
    
    // load the TSS, a little work around about gcc -O
    // ltr(GD_TSS_BOOT); // may crash
    volatile uint16_t tss = GD_TSS_BOOT;
    __ltr(tss);

    // print_pgdir();
}

static void *
__boot_page_alloc(size_t size)
{
    return boot_alloc(size, PGSIZE, 0);
}

int
mem_early_init(void)
{
    memmap_process_e820();
    boot_alloc_init();

    mmu_init();

    /* Mark the supervisor area and boot alloc area reserved */
    memmap_append(kern_start, kern_end, MEMMAP_FLAG_RESERVED);

    frame_sys_early_init_struct(nr_pages, __boot_page_alloc);
    valloc_early_init_struct();
    
    uintptr_t start, end;
    boot_alloc_get_area(&start, &end);

    memmap_append(start, end, MEMMAP_FLAG_RESERVED);
    memmap_process(1);

    return 0;
}
