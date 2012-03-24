#include <runtime/page.h>
#include <mach.h>

struct area_s
{
	size_t start;
	size_t end;

	int free;
};

struct area_node_s
{
	int rank;
	struct area_node_s *parent, *left, *right;

	size_t start, end;
	size_t max_free;
	
	struct area_s area;
};

typedef struct area_node_s area_node_s;
typedef area_node_s *area_node_t;

static area_node_t __root;
static uintptr_t   __start;
static uintptr_t   __end, __fend;

static area_node_t free_head;
#define INIT_FREE_NODES_COUNT 256
static area_node_s init_free_nodes[INIT_FREE_NODES_COUNT];

static area_node_t
__area_node_new(void)
{
	area_node_t result = free_head;
	if (result) free_head = result->right;
	return result;
}

static void
__area_node_free(area_node_t node)
{
	node->right = free_head;
	free_head = node;
}

static void *__palloc(size_t num);
static int   __pfree(void *addr);

void *
palloc(size_t num)
{
	if (free_head == NULL)
	{
		 return NULL;
	}
	else if (free_head->right == NULL)
	{
		size_t num = 1;
		/* only one node left -- use it to alloc a block of node array */
		area_node_t nodes = __palloc(num);
		if (nodes == NULL)
		{
			/* maybe to brk? */
			return NULL;
		}
		int i;
		for (i = 0; i < (num << __PGSHIFT) / sizeof(area_node_s); ++ i)
		{
			nodes[i].right = nodes + i + 1;
		}
		nodes[i - 1].right = free_head;
		free_head = nodes;
	}

	return __palloc(num);
}

void
pfree(void *addr)
{
	__pfree(addr);
}

#include <lib/low_io.h>

void
page_init(void *start, void *end)
{
	__root = NULL;
	__start = (uintptr_t)start;
	__fend = __end = (uintptr_t)end;
	int i;
	for (i = 0; i < INIT_FREE_NODES_COUNT; ++ i)
	{
		init_free_nodes[i].right = init_free_nodes + i + 1;
	}
	init_free_nodes[i - 1].right = NULL;
	free_head = init_free_nodes;
}

void *
sbrk(size_t amount)
{
	if (brk((void *)__fend + amount))
		return NULL;
	else return (void *)__fend;
}

int
brk(void *end)
{
	if ((uintptr_t)end > __end)
	{
		/* enlarge */
	}
	else if ((uintptr_t)end <= __end - __PGSIZE)
	{
		/* shrink */
	}

	__fend = (uintptr_t)end;
	return 0;
}

/* ALLOCATOR ================================================== */

#define __RBT_NodeType area_node_t
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
		struct area_s area = (a)->area;	\
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
#define __RBT_SetLeftChild(n,c)  do { (n)->left = (c); if ((c) != __RBT_NodeNull) (c)->parent = (n); area_update_inv(n); } while (0)
#define __RBT_SetRightChild(n,c) do { (n)->right = (c); if ((c) != __RBT_NodeNull) (c)->parent = (n); area_update_inv(n); } while (0)
#define __RBT_SetChildren(n,l,r) do { (n)->left = (l); if ((l) != __RBT_NodeNull) (l)->parent = (n); (n)->right = (r); if ((r) != __RBT_NodeNull) (r)->parent = (n); area_update_inv(n); } while (0)
#define __RBT_SetLeftChildFromRightChild(n,p)							\
	do { (n)->left = (p)->right; if ((n)->left) (n)->left->parent = (n); area_update_inv(n); } while (0)
#define __RBT_SetRightChildFromLeftChild(n,p)							\
	do { (n)->right = (p)->left; if ((n)->right) (n)->right->parent = (n); area_update_inv(n); } while (0)
#define __RBT_ThrowException(msg)				\
	do { } while (0)
#define __RBT_NeedFixUp 1

static void area_update_inv(area_node_t area);

#include <algo/rbt_algo.h>

static void
area_update_inv(area_node_t node)
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

static void *
__palloc(size_t num)
{
	if (num == 0) return NULL;
	area_node_t node = __root;

	area_node_t n = NULL;
   
	if (node == NULL)
	{
		n = __area_node_new();
		n->area.start = 0;
		n->area.end   = num;
	}
	else if (node->max_free < num)
	{
		if (node->start >= num)
		{
			n = __area_node_new();
			n->area.start = 0;
			n->area.end   = num;
		}
		else if (((__end - __start) >> __PGSHIFT) - node->end >= num)
		{
			n = __area_node_new();
			n->area.start = node->end;
			n->area.end   = n->area.start + num;
		}
		else return NULL;
	}

	while (n == NULL)
	{
		if (node->left)
		{
			if (node->left->max_free >= num)
			{
				node = node->left;
				continue;
			}
			else if (node->area.start - node->left->end >= num)
			{
				n = __area_node_new();
				n->area.start = node->left->end;
				n->area.end   = n->area.start + num;
				break;
			}
		}

		if (node->right)
		{
			if (node->right->max_free >= num)
			{
				node = node->right;
				continue;
			}
			else if (node->right->start - node->area.end >= num)
			{
				n = __area_node_new();
				n->area.start = node->area.end;
				n->area.end   = n->area.start + num;
				break;
			}
		}

		/* should not happen */
		return NULL;
	}

	__root = __RBT_Insert(node, n);

	return (void *)(__start + (n->area.start << __PGSHIFT));
}

static int
__pfree(void *addr)
{
	area_node_t node = __root;
	size_t start = ((uintptr_t)addr - __start) >> __PGSHIFT;
	
	while (1)
	{
		if (node == NULL) break;

		if (node->area.start <= start && node->area.end > start) break;
		if (node->area.start > start)
			node = node->left;
		else node = node->right;
	}

	if (node == NULL || node->area.start != start) return -1;
	
	__root = __RBT_Remove(node, &node, start);
	__area_node_free(node);

	return 0;
}
