#include <cpu.h>
#include <string.h>
#include <error.h>
#include <proc.h>
#include <user.h>
#include <page.h>
#include <malloc.h>
#include <arch/user.h>
#include <user/tls.h>

#include "mem.h"

int
user_mm_arch_init(user_mm_t mm, uintptr_t *start, uintptr_t *end)
{
	page_t pgdir_page = page_alloc_atomic(1);
	if (pgdir_page == NULL) return -E_NO_MEM;
	uintptr_t pgdir_phys = PAGE_TO_PHYS(pgdir_page);

	mm->arch.cr3   = pgdir_phys;
	mm->arch.pgdir = VADDR_DIRECT(pgdir_phys);
	/* copy from the scratch */
	memset(mm->arch.pgdir, 0, PGSIZE);
	memmove(mm->arch.pgdir + PGX(PHYSBASE), pgdir_scratch + PGX(PHYSBASE), (NPGENTRY - PGX(PHYSBASE)) * sizeof(pgd_t));
	mm->arch.pgdir[PGX(VPT)] = pgdir_phys | PTE_W | PTE_P;
	mm->arch.mmio_root = NULL;
	
	return 0;
}

int
user_mm_arch_copy_page(user_mm_t mm, uintptr_t addr, uintptr_t phys, int flag)
{
	pte_t *pte = get_pte(mm->arch.pgdir, addr, 1);
	if (!(*pte & PTE_P))
	{
		uintptr_t phys = PAGE_TO_PHYS(page_alloc_atomic(1));
		*pte = phys | PTE_W | PTE_U | PTE_P;
	}
	if (phys == 0)
	{
		memset(VADDR_DIRECT(PTE_ADDR(*pte)), 0, PGSIZE);
	}
	else
	{
		memmove(VADDR_DIRECT(PTE_ADDR(*pte)), VADDR_DIRECT(phys), PGSIZE);
	}
	return 0;
}

int
user_mm_arch_copy(user_mm_t mm, uintptr_t addr, void *src, size_t size)
{
	uintptr_t ptr = addr;
	uintptr_t end = ptr + size;
	char *__src = (char *)src - PGOFF(ptr);
	ptr -= PGOFF(ptr);

	while (ptr < end)
	{
		pte_t *pte = get_pte(mm->arch.pgdir, ptr, 0);
		if (pte == NULL) return -E_INVAL;
		if ((*pte & (PTE_P | PTE_W | PTE_U)) != (PTE_P | PTE_W | PTE_U)) return -E_INVAL;

		if (ptr < addr)
		{
			if (ptr + PGSIZE >= end)
			{
				memmove(VADDR_DIRECT(PTE_ADDR(*pte) + PGOFF(addr)),
						__src + PGOFF(addr), size);
			}
			else
			{
				memmove(VADDR_DIRECT(PTE_ADDR(*pte) + PGOFF(addr)),
						__src + PGOFF(addr), PGSIZE - PGOFF(addr));
			}
		}
		else
		{
			if (ptr + PGSIZE >= end)
			{
				memmove(VADDR_DIRECT(PTE_ADDR(*pte)),
						__src, end - ptr);
			}
			else
			{
				memmove(VADDR_DIRECT(PTE_ADDR(*pte)),
						__src, PGSIZE);
			}
		}

		ptr   += PGSIZE;
		__src += PGSIZE;
	}
	
	return 0;
}

void
user_arch_save_context(proc_t proc)
{ }

void
user_arch_restore_context(proc_t proc)
{
	/* change the gs base for TLS */
	__write_msr(0xC0000101, proc->usr_thread->tls_u);
	__lcr3(proc->usr_mm->arch.cr3);
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
#define __RBT_SetupNewRedNode(n) do {								\
		n->parent = n->left = n->right = NULL;						\
		n->rank = -1;												\
		n->max_free = 0;											\
		n->start = n->area.start;									\
		n->end   = n->area.end;										\
	} while (0);

#define __RBT_SwapNodeContent(a,b) do {			\
		struct user_area_s area = (a)->area;	\
		(a)->area = (b)->area;					\
		(b)->area = area;						\
	} while (0)
#define __RBT_GetRank(n) ((n) == __RBT_NodeNull ? 1 : (n)->rank)
#define __RBT_SetRank(n,r)  do { (n)->rank = (r); } while (0)
#define __RBT_CompareKey(k,n) ((k) == (n)->area.start ? 0 : ((k) < (n)->area.start ? -1 : 1))
#define __RBT_CompareNode(n1,n2) ((n1)->area.start == (n2)->area.start ? 0 : ((n1)->area.start < (n2)->area.start ? -1 : 1))
#define __RBT_AcquireParentAndDir(n,p,d)					\
	 {														\
		  (p) = (n)->parent;								\
		  (d) = (p) == __RBT_NodeNull ? DIR_ROOT :			\
			   ((p)->left == (n) ? DIR_LEFT : DIR_RIGHT);	\
	 }
#define __RBT_SetRoot(n) do { n->parent = NULL; } while (0)
#define __RBT_GetLeftChild(n)    ((n)->left)
#define __RBT_GetRightChild(n)   ((n)->right)
#define __RBT_SetLeftChild(n,c)  do { (n)->left = (c); if ((c) != __RBT_NodeNull) (c)->parent = (n); user_area_update_inv(n); } while (0)
#define __RBT_SetRightChild(n,c) do { (n)->right = (c); if ((c) != __RBT_NodeNull) (c)->parent = (n); user_area_update_inv(n); } while (0)
#define __RBT_SetChildren(n,l,r) do { (n)->left = (l); if ((l) != __RBT_NodeNull) (l)->parent = (n); (n)->right = (r); if ((r) != __RBT_NodeNull) (r)->parent = (n); user_area_update_inv(n); } while (0)
#define __RBT_SetLeftChildFromRightChild(n,p)							\
	do { (n)->left = (p)->right; if ((n)->left) (n)->left->parent = (n); user_area_update_inv(n); } while (0)
#define __RBT_SetRightChildFromLeftChild(n,p)							\
	do { (n)->right = (p)->left; if ((n)->right) (n)->right->parent = (n); user_area_update_inv(n); } while (0)
#define __RBT_ThrowException(msg)				\
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

int
user_mm_arch_mmio_open(user_mm_t mm, uintptr_t addr, size_t size, uintptr_t *result)
{
	if (addr & (PGSIZE - 1)) return -1;
	if ((size & (PGSIZE - 1)) || size == 0) return -1;
	user_area_node_t node = mm->arch.mmio_root;
	size >>= PGSHIFT;

	user_area_node_t n = NULL;
   
	if (node == NULL)
	{
		n = kmalloc(sizeof(user_area_node_s));
		n->area.start = 0;
		n->area.end   = size;
	}
	else if (node->max_free < size)
	{
		if (node->start >= size)
		{
			n = kmalloc(sizeof(user_area_node_s));
			n->area.start = 0;
			n->area.end   = size;
		}
		else if ((UMMIO_SIZE >> PGSHIFT) - node->end >= size)
		{
			n = kmalloc(sizeof(user_area_node_s));
			n->area.start = node->end;
			n->area.end   = n->area.start + size;
		}
		else return -1;
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
				n->area.start = node->area.end;
				n->area.end   = n->area.start + size;
				break;
			}
		}

		/* should not happen */
		return -1;
	}

	/* map job */
	size_t i;
	for (i = n->area.start; i < n->area.end; ++ i)
	{
		uintptr_t la = i << PGSHIFT;
		pte_t *pte = get_pte(mm->arch.pgdir, la, 1);
		/* FIXME: process no mem */
		*pte = (addr + ((i - n->area.start) << PGSHIFT)) | PTE_W | PTE_U | PTE_P;
	}
	
	mm->arch.mmio_root = __RBT_Insert(node, n);

	*result = (n->area.start << PGSHIFT);
	return 0;
}

int
user_mm_arch_mmio_close(user_mm_t mm, uintptr_t addr)
{
	user_area_node_t node = mm->arch.mmio_root;
	size_t start = addr >> PGSHIFT;
	
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
		uintptr_t la = i << PGSHIFT;
		pte_t *pte = get_pte(mm->arch.pgdir, la, 0);
		/* FIXME: process no mem */
		*pte = 0;
	}
	
	mm->arch.mmio_root = __RBT_Remove(node, &node, start);
	kfree(node);

	return 0;
}
