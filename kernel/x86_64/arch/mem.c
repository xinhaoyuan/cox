#define DEBUG_COMPONENT DBG_MEM

#include <types.h>
#include <string.h>
#include <frame.h>
#include <irq.h>
#include <spinlock.h>
#include <lib/buddy.h>
#include <lib/low_io.h>
#include <proc.h>
#include <ips.h>
#include <arch/paging.h>
#include <asm/cpu.h>
#include <debug.h>

#include "memmap.h"
#include "e820.h"
#include "boot_alloc.h"
#include "sysconf_x86.h"
#include "ioapic.h"
#include "mem.h"

uintptr_t kern_start;
uintptr_t kern_end;
uintptr_t phys_end;
uintptr_t nr_pages;

// virtual address of boot-time page directory
pgd_t *pgdir_scratch = NULL;
static pgd_t *boot_pgdir = NULL;
// physical address of boot-time page directory
static uintptr_t boot_cr3;

pte_t * const vpt = (pte_t *)VPT;
pmd_t * const vmd = (pmd_t *)PGADDR(PGX(VPT), PGX(VPT), 0, 0, 0);
pud_t * const vud = (pud_t *)PGADDR(PGX(VPT), PGX(VPT), PGX(VPT), 0, 0);
pgd_t * const vgd = (pgd_t *)PGADDR(PGX(VPT), PGX(VPT), PGX(VPT), PGX(VPT), 0);

static struct taskstate ts_boot = {0};

/* *
 * Global Descriptor Table:
 *
 * The kernel and user segments are identical (except for the DPL). To load
 * the %ss register, the CPL must equal the DPL. Thus, we must duplicate the
 * segments for the user and the kernel. Defined as follows:
 *   - 0x0 :  unused (always faults -- for trapping NULL far pointers)
 *   - 0x10:  kernel code segment
 *   - 0x20:  kernel data segment
 *   - 0x30:  user code segment
 *   - 0x40:  user data segment
 *   - 0x50:  defined for boot tss, initialized in gdt_init
 * */
struct segdesc gdt[SEG_COUNT + 1] = {
    SEG_NULL,
    [SEG_KTEXT]    = SEG(STA_X | STA_R, DPL_KERNEL),
    [SEG_KDATA]    = SEG(STA_W, DPL_KERNEL),
    [SEG_UTEXT]    = SEG(STA_X | STA_R, DPL_USER),
    [SEG_UDATA]    = SEG(STA_W, DPL_USER),
    [SEG_TSS_BOOT] = SEG_NULL,
};

struct pseudodesc gdt_pd = {
    sizeof(gdt) - 1, (uintptr_t)gdt
};

/* INIT CODE ================================================== */

static pgd_t *
boot_get_pgd(pgd_t *pgdir, uintptr_t la, bool create) {
    return &pgdir[PGX(la)];
}

static pud_t *
boot_get_pud(pgd_t *pgdir, uintptr_t la, bool create) {
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
boot_get_pte(pgd_t *pgdir, uintptr_t la, bool create) {
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
boot_map_segment(pgd_t *pgdir, uintptr_t la, size_t size, uintptr_t pa, uint32_t perm) {
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

/* *
 * lgdt - load the global descriptor table register and reset the
 * data/code segement registers for kernel.
 * */
static inline void
lgdt(struct pseudodesc *pd) {
    asm volatile ("lgdt (%0)" :: "r" (pd));
    asm volatile ("movw %%ax, %%es" :: "a" (KERNEL_DS));
    asm volatile ("movw %%ax, %%ds" :: "a" (KERNEL_DS));
    // reload cs & ss
    asm volatile (
        "pushq %%rax;"
        "movq %%rsp, %%rax;"            // move %rsp to %rax
        "pushq %1;"                     // push %ss
        "pushq %%rax;"                  // push %rsp
        "pushfq;"                       // push %rflags
        "pushq %0;"                     // push %cs
        "call 1f;"                      // push %rip
        "jmp 2f;"
        "1: iretq; 2:"
        "popq %%rax;"
        :: "i" (KERNEL_CS), "i" (KERNEL_DS));
}

//perm2str - use string 'u,r,w,-' to present the permission
static const char *
perm2str(int perm) {
    static char str[4];
    str[0] = (perm & PTE_U) ? 'u' : '-';
    str[1] = 'r';
    str[2] = (perm & PTE_W) ? 'w' : '-';
    str[3] = '\0';
    return str;
}

//get_pgtable_items - In [left, right] range of PDT or PT, find a continuous linear addr space
//                  - (left_store*X_SIZE~right_store*X_SIZE) for PDT or PT
//                  - X_SIZE=PTSIZE=4M, if PDT; X_SIZE=PGSIZE=4K, if PT
// paramemters:
//  left:        no use ???
//  right:       the high side of table's range
//  start:       the low side of table's range
//  table:       the beginning addr of table
//  left_store:  the pointer of the high side of table's next range
//  right_store: the pointer of the low side of table's next range
// return value: 0 - not a invalid item range, perm - a valid item range with perm permission 
static int
get_pgtable_items(size_t left, size_t right, size_t start, uintptr_t *table, size_t *left_store, size_t *right_store) {
    if (start >= right) {
        return 0;
    }
    while (start < right && !(table[start] & PTE_P)) {
        start ++;
    }
    if (start < right) {
        if (left_store != NULL) {
            *left_store = start;
        }
        int perm = (table[start ++] & PTE_USER);
        while (start < right && (table[start] & PTE_USER) == perm) {
            start ++;
        }
        if (right_store != NULL) {
            *right_store = start;
        }
        return perm;
    }
    return 0;
}

static void
print_pgdir_sub(int deep, size_t left, size_t right, char *s1[], size_t s2[], uintptr_t *s3[]) {
    if (deep > 0) {
        uint32_t perm;
        size_t l, r = left;
        while ((perm = get_pgtable_items(left, right, r, s3[0], &l, &r)) != 0) {
            DEBUG(s1[0], r - l);
            size_t lb = l * s2[0], rb = r * s2[0];
            if ((lb >> 32) & 0x8000) {
                lb |= (0xFFFFLLU << 48);
                rb |= (0xFFFFLLU << 48);
            }
            DEBUG(" %016llx-%016llx %016llx %s\n", lb, rb, rb - lb, perm2str(perm));
            print_pgdir_sub(deep - 1, l * NPGENTRY, r * NPGENTRY, s1 + 1, s2 + 1, s3 + 1);
        }
    }
}

//print_pgdir - print the PDT&PT
void
print_pgdir(void) {
    char *s1[] = {
        "PGD          (%09x)",
        " |-PUD       (%09x)",
        " |--|-PMD    (%09x)",
        " |--|--|-PTE (%09x)",
    };
    size_t s2[] = {PUSIZE, PMSIZE, PTSIZE, PGSIZE};
    uintptr_t *s3[] = {vgd, vud, vmd, vpt};
    DEBUG("-------------------- BEGIN --------------------\n");
    print_pgdir_sub(sizeof(s1) / sizeof(s1[0]), 0, NPGENTRY, s1, s2, s3);
    DEBUG("--------------------- END ---------------------\n");
}

static void
mmu_init(void)
{
    int i;
        
    pgdir_scratch = boot_pgdir = boot_alloc(PGSIZE, PGSIZE, 0);
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
    /* 6 for start of dynamic segment */
    for (i = 6; i <= SEG_COUNT; ++ i)
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

static int
config_by_memmap(uintptr_t num)
{
    return memmap_test(num << PGSHIFT, (num << PGSHIFT) + PGSIZE);
}

static int
kv_config(uintptr_t num)
{ return 1; }

static struct buddy_context_s kv_buddy;
static spinlock_s kv_lock;
static spinlock_s mmio_fix_lock;

static void *
__boot_page_alloc(size_t size)
{
    return boot_alloc(size, PGSIZE, 0);
}

int
memory_init(void)
{
    memmap_process_e820();
    boot_alloc_init();

    mmu_init();

    /* Mark the supervisor area and boot alloc area reserved */
    memmap_append(kern_start, kern_end, MEMMAP_FLAG_RESERVED);

    frame_sys_early_init_struct(nr_pages, __boot_page_alloc);
    kv_buddy.node = boot_alloc(BUDDY_CALC_ARRAY_SIZE(KVSIZE >> PGSHIFT) * sizeof(struct buddy_node_s), PGSIZE, 0);
    buddy_build(&kv_buddy, KVSIZE >> PGSHIFT, kv_config);
    
    uintptr_t start, end;
    boot_alloc_get_area(&start, &end);

    memmap_append(start, end, MEMMAP_FLAG_RESERVED);
    memmap_process(1);

    /* Initialize the page allocator with free areas */

    frame_sys_early_init_layout(config_by_memmap);
    spinlock_init(&kv_lock);
    spinlock_init(&mmio_fix_lock);

    return 0;
}

/* INIT CODE END ================================================== */

static pgd_t *
get_pgd(pgd_t *pgdir, uintptr_t la)
{
    return &pgdir[PGX(la)];
}

static pud_t *
get_pud(pgd_t *pgdir, uintptr_t la, unsigned int flags)
{
    pgd_t *pgdp;
    if ((pgdp = get_pgd(pgdir, la)) == NULL)
    {
        return NULL;
    }
    
    if (!(*pgdp & PTE_P))
    {
        pud_t *pud;
        frame_t frame;
        
        if ((frame = frame_alloc(1, flags)) == NULL)
        {
            return NULL;
        }
        
        uintptr_t pa = FRAME_TO_PHYS(frame);
        pud = VADDR_DIRECT(pa);
        memset(pud, 0, PGSIZE);
        *pgdp = pa | PTE_U | PTE_W | PTE_P;
    }
    
    return &((pud_t *)VADDR_DIRECT(PGD_ADDR(*pgdp)))[PUX(la)];
}

static pmd_t *
get_pmd(pgd_t *pgdir, uintptr_t la, unsigned int flags)
{
    pud_t *pudp;
    
    if ((pudp = get_pud(pgdir, la, flags)) == NULL)
    {
        return NULL;
    }
    
    if (!(*pudp & PTE_P))
    {
        pmd_t *pmd;
        frame_t frame;
        
        if ((frame = frame_alloc(1, flags)) == NULL)
        {
            return NULL;
        }
        
        uintptr_t pa = FRAME_TO_PHYS(frame);
        pmd = VADDR_DIRECT(pa);
        memset(pmd, 0, PGSIZE);
        *pudp = pa | PTE_U | PTE_W | PTE_P;
    }
    
    return &((pmd_t *)VADDR_DIRECT(PUD_ADDR(*pudp)))[PMX(la)];
}

pte_t *
get_pte(pgd_t *pgdir, uintptr_t la, unsigned int flags)
{
    pmd_t *pmdp;
    if ((pmdp = get_pmd(pgdir, la, flags)) == NULL)
    {
        return NULL;
    }
    
    if (!(*pmdp & PTE_P))
    {
        pte_t *pte;
        frame_t frame;
        
        if ((frame = frame_alloc(1, flags)) == 0)
        {
            return NULL;
        }
        
        uintptr_t pa = FRAME_TO_PHYS(frame);
        pte = VADDR_DIRECT(pa);
        memset(pte, 0, PGSIZE);
        *pmdp = pa | PTE_U | PTE_W | PTE_P;
    }

    return &((pte_t *)VADDR_DIRECT(PMD_ADDR(*pmdp)))[PTX(la)];
}

void
pgflt_handler(unsigned int err, uintptr_t la, uintptr_t pc)
{
    if (PHYSBASE <= la && la < PHYSBASE + PHYSSIZE)
    {
        if (err & 4)
        {
            /* MMIO AREA IS NOT ALLOWED FOR USER */
            PANIC("MMIO FROM USER\n");
        }
        else
        {
            /* KERNEL MMIO PG FAULT, fix it */
            la = la & ~(uintptr_t)0x1fffff;
           
            spinlock_acquire(&mmio_fix_lock);
            pmd_t *pmd = get_pmd(boot_pgdir, la, FA_ATOMIC);
            spinlock_release(&mmio_fix_lock);

            /* All mmio mappings are the same across all page tables, so just
             * modify it for one place */
            if (pmd)
                *pmd = PADDR_DIRECT(la) | PTE_PS | PTE_W | PTE_P;
            else
            {
                PANIC("Cannot alloc frames for kernel MMIO mapping\n");
            }
        }
    }
    /* in user area ? */
    else if (la < UTOP)
    {
        if (current->type != PROC_TYPE_USER)
        {
            PANIC("user area page fault by non-user thread %p\nLA: %p, PC: %p\n",
                  current, la, pc);
        }
        else
        {
            PANIC("PGFLT IN USERSPACE");
            // user_thread_pgflt_handler(current, err, la, pc);
        }
    }
}

void *
valloc(size_t num)
{
    int irq = irq_save();
    spinlock_acquire(&kv_lock);

    buddy_node_id_t result = buddy_alloc(&kv_buddy, num);

    spinlock_release(&kv_lock);
    irq_restore(irq);
    
    if (result == BUDDY_NODE_ID_NULL) return NULL;
    else return (void *)(KVBASE + (result << PGSHIFT));
}

void
vfree(void *addr)
{
    if (addr == NULL) return;
    if (KVBASE > (uintptr_t)addr || (uintptr_t)addr >= KVBASE + KVSIZE) return;

    int irq = irq_save();
    spinlock_acquire(&kv_lock);
    
    buddy_free(&kv_buddy, ((uintptr_t)addr - KVBASE) >> PGSHIFT);
    
    spinlock_release(&kv_lock);
    irq_restore(irq);
}
