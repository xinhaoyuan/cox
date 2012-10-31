#include <asm/cpu.h>
#include <string.h>
#include <error.h>
#include <proc.h>
#include <user.h>
#include <frame.h>
#include <kmalloc.h>
#include <arch/user.h>
#include <ips.h>
#include <tls.h>

#include "arch/mem.h"

int
user_proc_arch_init(user_proc_t user_proc, uintptr_t *start, uintptr_t *end)
{
    frame_t pgdir_frame = frame_alloc(1);
    if (pgdir_frame == NULL) return -E_NO_MEM;
    
    uintptr_t pgdir_phys = FRAME_TO_PHYS(pgdir_frame);

    mutex_init(&user_proc->arch.pgflt_lock);
    user_proc->arch.cr3   = pgdir_phys;
    user_proc->arch.pgdir = VADDR_DIRECT(pgdir_phys);
    
    /* Copy from the scratch pgtab */
    memset(user_proc->arch.pgdir, 0, _MACH_PAGE_SIZE);
    memmove(user_proc->arch.pgdir + PGX(PHYSBASE), pgdir_kernel + PGX(PHYSBASE), (NPGENTRY - PGX(PHYSBASE)) * sizeof(pgd_t));
    user_proc->arch.pgdir[PGX(VPT)] = pgdir_phys | PTE_W | PTE_P;
    user_proc->arch.mmio_root = NULL;
    
    return 0;
}

int
user_proc_arch_copy_page_to_user(user_proc_t user_proc, uintptr_t addr, uintptr_t phys, unsigned int flags)
{
    pte_t *pte = get_pte(user_proc->arch.pgdir, addr, 1);
    if (pte == NULL) return -E_NO_MEM;

    if (!(*pte & PTE_P))
    {
        frame_t frame  = frame_alloc(1);
        if (frame == NULL) return -E_NO_MEM;
        
        uintptr_t phys = FRAME_TO_PHYS(frame);
        *pte = phys | PTE_W | PTE_U | PTE_P;
    }
    
    if (phys == 0)
        memset(VADDR_DIRECT(PTE_ADDR(*pte)), 0, _MACH_PAGE_SIZE);
    else memmove(VADDR_DIRECT(PTE_ADDR(*pte)), VADDR_DIRECT(phys), _MACH_PAGE_SIZE);
    
    return 0;
}

int
user_proc_arch_copy_to_user(user_proc_t user_proc, uintptr_t addr, void *src, size_t size)
{
    uintptr_t ptr = addr;
    uintptr_t end = ptr + size;
    char *__src = (char *)src - PGOFF(ptr);
    ptr -= PGOFF(ptr);

    while (ptr < end)
    {
        pte_t *pte = get_pte(user_proc->arch.pgdir, ptr, 1);
        if (pte == NULL) return -E_INVAL;
        if ((*pte & (PTE_P | PTE_W | PTE_U)) != (PTE_P | PTE_W | PTE_U)) return -E_INVAL;

        if (ptr < addr)
        {
            if (ptr + _MACH_PAGE_SIZE >= end)
            {
                memmove(VADDR_DIRECT(PTE_ADDR(*pte) + PGOFF(addr)),
                        __src + PGOFF(addr), size);
            }
            else
            {
                memmove(VADDR_DIRECT(PTE_ADDR(*pte) + PGOFF(addr)),
                        __src + PGOFF(addr), _MACH_PAGE_SIZE - PGOFF(addr));
            }
        }
        else
        {
            if (ptr + _MACH_PAGE_SIZE >= end)
            {
                memmove(VADDR_DIRECT(PTE_ADDR(*pte)),
                        __src, end - ptr);
            }
            else
            {
                memmove(VADDR_DIRECT(PTE_ADDR(*pte)),
                        __src, _MACH_PAGE_SIZE);
            }
        }

        ptr   += _MACH_PAGE_SIZE;
        __src += _MACH_PAGE_SIZE;
    }
    
    return 0;
}

int
user_proc_arch_brk(user_proc_t user_proc, uintptr_t end)
{
    int ret;
    
    if (end > user_proc->end)
    {
        /* enlarge */
        /* XXX: alloc by demand */
        uintptr_t cur = user_proc->end;
        while (cur != end)
        {
            if ((ret = user_proc_arch_copy_page_to_user(user_proc, cur, 0, 0)) != 0)
                return ret;
            cur += _MACH_PAGE_SIZE;
        }
    }
    else if (end < user_proc->end)
    {
        /* compact */
        uintptr_t cur = end;
        while (cur != user_proc->end)
        {
            pte_t *pte = get_pte(user_proc->arch.pgdir, cur, 0);
            /* XXX: to consider swap mach? */
            if (pte != NULL && (*pte & PTE_P))
                frame_put(PHYS_TO_FRAME(PTE_ADDR(*pte)));
            cur += _MACH_PAGE_SIZE;
        }
    }
    
    return 0;
}

/* USER MMIO */

struct user_area_s
{
    size_t start;
    size_t end;
    
    union
    {
        struct
        {
            uintptr_t physaddr;
        } mmio;
    };
    
    int free;
};

struct user_area_node_s
{
    int rank;
    struct user_area_node_s *parent, *left, *right;

    size_t start, end;
    size_t max_free;
    
    struct user_area_s area;
};

typedef struct user_area_node_s user_area_node_s;
typedef user_area_node_s *user_area_node_t;

#define __RBT_NodeType user_area_node_t
#define __RBT_KeyType size_t
#define __RBT_NodeNull NULL
#define __RBT_SetupNewRedNode(n) do {                               \
        n->parent = n->left = n->right = NULL;                      \
        n->rank = -1;                                               \
        n->max_free = 0;                                            \
        n->start = n->area.start;                                   \
        n->end   = n->area.end;                                     \
    } while (0);

#define __RBT_SwapNodeContent(a,b) do {         \
        struct user_area_s area = (a)->area;    \
        (a)->area = (b)->area;                  \
        (b)->area = area;                       \
    } while (0)
#define __RBT_GetRank(n) ((n) == __RBT_NodeNull ? 1 : (n)->rank)
#define __RBT_SetRank(n,r)  do { (n)->rank = (r); } while (0)
#define __RBT_CompareKey(k,n) ((k) == (n)->area.start ? 0 : ((k) < (n)->area.start ? -1 : 1))
#define __RBT_CompareNode(n1,n2) ((n1)->area.start == (n2)->area.start ? 0 : ((n1)->area.start < (n2)->area.start ? -1 : 1))
#define __RBT_AcquireParentAndDir(n,p,d)                    \
     {                                                      \
          (p) = (n)->parent;                                \
          (d) = (p) == __RBT_NodeNull ? DIR_ROOT :          \
               ((p)->left == (n) ? DIR_LEFT : DIR_RIGHT);   \
     }
#define __RBT_SetRoot(n) do { n->parent = NULL; } while (0)
#define __RBT_GetLeftChild(n)    ((n)->left)
#define __RBT_GetRightChild(n)   ((n)->right)
#define __RBT_SetLeftChild(n,c)  do { (n)->left = (c); if ((c) != __RBT_NodeNull) (c)->parent = (n); user_area_update_inv(n); } while (0)
#define __RBT_SetRightChild(n,c) do { (n)->right = (c); if ((c) != __RBT_NodeNull) (c)->parent = (n); user_area_update_inv(n); } while (0)
#define __RBT_SetChildren(n,l,r) do { (n)->left = (l); if ((l) != __RBT_NodeNull) (l)->parent = (n); (n)->right = (r); if ((r) != __RBT_NodeNull) (r)->parent = (n); user_area_update_inv(n); } while (0)
#define __RBT_SetLeftChildFromRightChild(n,p)                           \
    do { (n)->left = (p)->right; if ((n)->left) (n)->left->parent = (n); user_area_update_inv(n); } while (0)
#define __RBT_SetRightChildFromLeftChild(n,p)                           \
    do { (n)->right = (p)->left; if ((n)->right) (n)->right->parent = (n); user_area_update_inv(n); } while (0)
#define __RBT_ThrowException(msg)               \
    do { } while (0)
#define __RBT_NeedFixUp 1

static void user_area_update_inv(user_area_node_t area);

#include <algo/rbt_algo.h>

static void
user_area_update_inv(user_area_node_t node)
{
    node->max_free = 0;
    if (node->left)
    {
        node->start = node->left->start;
        if (node->left->max_free > node->max_free)
            node->max_free = node->left->max_free;
        if (node->area.start - node->left->end > node->max_free)
            node->max_free = node->area.start - node->left->end;
    }
    else node->start = node->area.start;
    if (node->right)
    {
        node->end = node->right->end;
        if (node->right->max_free > node->max_free)
            node->max_free = node->right->max_free;
        if (node->right->start - node->area.end > node->max_free)
            node->max_free = node->right->start - node->area.end;
    }
    else node->end = node->area.end;
}

static user_area_node_t
user_area_node_fit(user_area_node_t node, size_t size)
{
    user_area_node_t n = NULL;
   
    if (node == NULL)
    {
        n = kmalloc(sizeof(user_area_node_s));
        if (n == NULL) return NULL;
        
        n->area.start = 0;
        n->area.end   = size;
    }
    else if (node->max_free < size)
    {
        if (node->start >= size)
        {
            n = kmalloc(sizeof(user_area_node_s));
            if (n == NULL) return NULL;
            
            n->area.start = 0;
            n->area.end   = size;
        }
        else if ((UMMIO_SIZE >> PGSHIFT) - node->end >= size)
        {
            n = kmalloc(sizeof(user_area_node_s));
            if (n == NULL) return NULL;
            
            n->area.start = node->end;
            n->area.end   = n->area.start + size;
        }
        else return NULL;
    }

    while (n == NULL)
    {
        if (node->left)
        {
            if (node->left->max_free >= size)
            {
                node = node->left;
                continue;
            }
            else if (node->area.start - node->left->end >= size)
            {
                n = kmalloc(sizeof(user_area_node_s));
                if (n == NULL) return NULL;
                
                n->area.start = node->left->end;
                n->area.end   = n->area.start + size;
                break;
            }
        }

        if (node->right)
        {
            if (node->right->max_free >= size)
            {
                node = node->right;
                continue;
            }
            else if (node->right->start - node->area.end >= size)
            {
                n = kmalloc(sizeof(user_area_node_s));
                if (n == NULL) return NULL;
                
                n->area.start = node->area.end;
                n->area.end   = n->area.start + size;
                break;
            }
        }

        /* should not happen */
        return NULL;
    }

    return n;
}

int
user_proc_arch_mmio_open(user_proc_t user_proc, uintptr_t addr, size_t size, uintptr_t *result)
{
    if (addr & (_MACH_PAGE_SIZE - 1)) return -1;
    if ((size & (_MACH_PAGE_SIZE - 1)) || size == 0) return -1;

    size >>= _MACH_PAGE_SHIFT;
    user_area_node_t n = user_area_node_fit(user_proc->arch.mmio_root, size);
    if (n == NULL) return -E_NO_MEM;
        
    /* MAP MEMORY PAGES */
    size_t i;
    for (i = n->area.start; i < n->area.end; ++ i)
    {
        uintptr_t la = UMMIO_BASE + (i << _MACH_PAGE_SHIFT);
        pte_t *pte = get_pte(user_proc->arch.pgdir, la, 1);
        if (pte == NULL)
        {
            kfree(n);
            return -E_NO_MEM;
        }
        /* FIXME: process no mem */
        *pte = (addr + ((i - n->area.start) << _MACH_PAGE_SHIFT)) | PTE_W | PTE_U | PTE_P;
    }
    
    user_proc->arch.mmio_root = __RBT_Insert(user_proc->arch.mmio_root, n);

    *result = UMMIO_BASE + (n->area.start << _MACH_PAGE_SHIFT);
    return 0;
}

int
user_proc_arch_mmio_close(user_proc_t user_proc, uintptr_t addr)
{
    user_area_node_t node = user_proc->arch.mmio_root;
    size_t start = (addr - UMMIO_BASE) >> PGSHIFT;
    
    while (1)
    {
        if (node == NULL) break;

        if (node->area.start <= start && node->area.end > start) break;
        if (node->area.start > start)
            node = node->left;
        else node = node->right;
    }

    if (node == NULL || node->area.start != start) return -1;

    /* Ummap job */
    size_t i;
    for (i = node->area.start; i < node->area.end; ++ i)
    {
        uintptr_t la = UMMIO_BASE + (i << PGSHIFT);
        pte_t *pte = get_pte(user_proc->arch.pgdir, la, 1);
        /* FIXME: process no mem */
        if (pte)
            *pte = 0;
    }
    
    user_proc->arch.mmio_root = __RBT_Remove(node, &node, start);
    kfree(node);

    return 0;
}
