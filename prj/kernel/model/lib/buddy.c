#include <bit.h>

#include "buddy.h"

/* A not that simple but tricky abstract buddy allocator, which would
 * manage a range of address space and allocate/free blocks. Now the
 * allocator can support up-to 52-bit as the range of address, which
 * match the page count in x86_64 */

/* static assertion */
static char __sa[sizeof(struct buddy_node_s) == 16 ? 0 : -1] __attribute__((unused));

#define BUDDY_TYPE_ROOT      0
#define BUDDY_TYPE_LEFT      1
#define BUDDY_TYPE_RIGHT     2

#define BUDDY_STATUS_FREE      0
#define BUDDY_STATUS_PARTIAL   1
#define BUDDY_STATUS_INVALID   2
#define BUDDY_STATUS_USED_L    3
#define BUDDY_STATUS_USED_R    4

#define BUDDY_LEFT_CHILD(id, lv)   ((id) - (1 << ((lv) - 1)))
#define BUDDY_RIGHT_CHILD(id, lv)  ((id) + (1 << ((lv) - 1)))
#define BUDDY_LEFT_PARENT(id, lv)  ((id) + (1 << (lv)))
#define BUDDY_RIGHT_PARENT(id, lv) ((id) - (1 << (lv)))
#define BUDDY_LEFT_TAIL(id, lv)    ((id) - (1 << (lv)) + 1)
#define BUDDY_RIGHT_TAIL(id, lv)   ((id) + (1 << (lv)) - 1)

/* The static constants */
static int BUDDY_INIT_COND = 0;
static int BUDDY_SN[256];
static int BUDDY_BITMAP_ALLOC[256][4];
static int BUDDY_BITMAP_FREE[256][4];

static inline void
add_to_clist(struct buddy_context_s *ctx, buddy_node_id_t node, int clevel)
{
	 if (ctx->clist[clevel] == BUDDY_NODE_ID_NULL)
	 {
		  ctx->node[node].next = ctx->node[node].prev = node;
		  ctx->clist[clevel] = node;

		  ctx->clist_bm |= 1 << clevel;
	 }
	 else
	 {
		  buddy_node_id_t next = ctx->clist[clevel];
		  buddy_node_id_t prev = ctx->node[next].prev;

		  ctx->node[node].next = next;
		  ctx->node[node].prev = prev;

		  ctx->node[prev].next = node;
		  ctx->node[next].prev = node;
	 }
}

static inline void
remove_from_clist(struct buddy_context_s *ctx, buddy_node_id_t node, int clevel)
{
	 if (ctx->node[node].next == node)
	 {
		  ctx->clist[clevel] = BUDDY_NODE_ID_NULL;
		  ctx->clist_bm ^= (1 << clevel);
	 }
	 else
	 {
		  buddy_node_id_t next = ctx->node[node].next;
		  buddy_node_id_t prev = ctx->node[node].prev;

		  ctx->node[next].prev = prev;
		  ctx->node[prev].next = next;

		  if (ctx->clist[clevel] == node)
			   ctx->clist[clevel] = next;
	 }
}

static inline void
add_to_slist(struct buddy_context_s *ctx, buddy_node_id_t node)
{
	 int sn = BUDDY_SN[ctx->node[node].bitmap];
	 if (ctx->slist[sn] == BUDDY_NODE_ID_NULL)
	 {
		  ctx->node[node].next = ctx->node[node].prev = node;
		  ctx->slist[sn] = node;

		  ctx->slist_bm |= 1 << sn;
	 }
	 else
	 {
		  buddy_node_id_t next = ctx->slist[sn];
		  buddy_node_id_t prev = ctx->node[next].prev;

		  ctx->node[node].next = next;
		  ctx->node[node].prev = prev;

		  ctx->node[prev].next = node;
		  ctx->node[next].prev = node;
	 }
}

static inline void
remove_from_slist(struct buddy_context_s *ctx, buddy_node_id_t node)
{
	 int sn = BUDDY_SN[ctx->node[node].bitmap];
	 if (ctx->node[node].next == node)
	 {
		  ctx->slist[sn] = BUDDY_NODE_ID_NULL;
		  ctx->slist_bm ^= (1 << sn);
	 }
	 else
	 {
		  buddy_node_id_t next = ctx->node[node].next;
		  buddy_node_id_t prev = ctx->node[node].prev;

		  ctx->node[next].prev = prev;
		  ctx->node[prev].next = next;

		  if (ctx->slist[sn] == node)
			   ctx->slist[sn] = next;
	 }
}


static int
buddy_build_internal(struct buddy_context_s *ctx, buddy_node_id_t start, int clevel, int type, buddy_initial_is_free_f config)
{	 
	 if (clevel > 0)
	 {
		  buddy_build_internal(ctx, start, clevel - 1, BUDDY_TYPE_LEFT, config);
	 }

	 buddy_node_id_t node = ctx->node_count ++;
	 ctx->node[node].type = type;

	 if (clevel > 0)
	 {
		  buddy_build_internal(ctx, start + (1 << (clevel + 1)), clevel - 1, BUDDY_TYPE_RIGHT, config);
		  
		  buddy_node_id_t left = BUDDY_LEFT_CHILD(node, clevel);
		  buddy_node_id_t right = BUDDY_RIGHT_CHILD(node, clevel);

		  if (ctx->node[left].status == BUDDY_STATUS_FREE &&
			  ctx->node[right].status == BUDDY_STATUS_FREE)
		  {
			   ctx->node[node].status = BUDDY_STATUS_FREE;
		  }
		  else
		  {
			   if (ctx->node[left].status == BUDDY_STATUS_FREE)
			   {
					add_to_clist(ctx, left, clevel - 1);
					ctx->node[node].status = BUDDY_STATUS_PARTIAL;
			   }
			   else if (ctx->node[right].status == BUDDY_STATUS_FREE)
			   {
					add_to_clist(ctx, right, clevel - 1);
					ctx->node[node].status = BUDDY_STATUS_PARTIAL;
			   }
			   else if (ctx->node[left].status == BUDDY_STATUS_INVALID &&
						ctx->node[right].status == BUDDY_STATUS_INVALID)
			   {
					ctx->node[node].status = BUDDY_STATUS_INVALID;
			   }
			   else
			   {
					ctx->node[node].status = BUDDY_STATUS_PARTIAL;
			   }
		  }
	 }
	 else
	 {
		  int i;
		  ctx->node[node].bitmap = 0;
		  for (i = 0; i != 4; ++ i)
			   if (!config(start + i))
					ctx->node[node].bitmap |= (3 << (i << 1));

		  if (ctx->node[node].bitmap == 255)
			   ctx->node[node].status = BUDDY_STATUS_INVALID;
		  if (ctx->node[node].bitmap == 0)
		  {
			   ctx->node[node].status = BUDDY_STATUS_FREE;
		  }
		  else
		  {
			   ctx->node[node].status = BUDDY_STATUS_PARTIAL;
			   add_to_slist(ctx, node);
		  }
	 }

	 return node;
}

int
buddy_build(struct buddy_context_s *ctx, buddy_node_id_t num, buddy_initial_is_free_f config)
{
	 if (ctx->node  == NULL ||
		 ctx->clist == NULL ||
		 ctx->slist == NULL)
		  return -1;

	 num = ((num - 1) | 3) + 1;
	 ctx->node_count  = 0;
	 ctx->clist_count = BUDDY_CALC_CLIST_COUNT(num);
	 if (ctx->clist_count > BUDDY_CLEVEL_COUNT_MAX) return -1;
	 ctx->clist_bm = 0;
	 ctx->slist_bm = 0;

	 int i;
	 for (i = 0; i != ctx->clist_count; ++ i)
		  ctx->clist[i] = BUDDY_NODE_ID_NULL;
	 for (i = 0; i != 4; ++ i)
		  ctx->slist[i] = BUDDY_NODE_ID_NULL;

	 buddy_node_id_t start = 0;
	 while (num > 0)
	 {
		  int level = BIT_SEARCH_LAST(num);
		  buddy_node_id_t root = buddy_build_internal(ctx, start, level - 2, BUDDY_TYPE_ROOT, config);
		  if (ctx->node[root].status == BUDDY_STATUS_FREE)
		  {
			   add_to_clist(ctx, root, level - 2);
		  }

		  start += 1 << level;
		  num   -= 1 << level;

		  ++ ctx->node_count;
	 }
	 return 0;
}

buddy_node_id_t
buddy_alloc(struct buddy_context_s *ctx, buddy_node_id_t num)
{
	 if (num <= 0) num = 1;

	 int i;
	 if (num < 4)
	 {
		  if ((ctx->slist_bm >> num) != 0)
		  {
			  i = num + BIT_SEARCH_FIRST(ctx->slist_bm >> num);
			  
			  buddy_node_id_t cur = ctx->slist[i];
			  remove_from_slist(ctx, cur);
			  int mask = BUDDY_BITMAP_ALLOC[ctx->node[cur].bitmap][num];
			  ctx->node[cur].status = BUDDY_STATUS_PARTIAL;
			  ctx->node[cur].bitmap |= mask;
			  add_to_slist(ctx, cur);
			  
			  return (BUDDY_LEFT_TAIL(cur, 0) << 1) + (BIT_SEARCH_FIRST(mask) >> 1);
		  }
	 }
	 
	 i = num <= 4 ? 0 : (BIT_SEARCH_LAST(num - 1) - 1);
	 if ((ctx->clist_bm >> i) == 0) return -1;
	 else i += BIT_SEARCH_FIRST(ctx->clist_bm >> i);
	 
	 if (ctx->clist[i] != BUDDY_NODE_ID_NULL)
	 {
		  buddy_node_id_t cur = ctx->clist[i];
		  remove_from_clist(ctx, cur, i);
			   
		  int remain = (1 << (i + 2)) - num;
		  ctx->node[cur].status = BUDDY_STATUS_USED_R;
			   
		  buddy_node_id_t sid = cur;
		  buddy_node_id_t lid = BUDDY_NODE_ID_NULL;
		  int lev = i;
		  while (remain)
		  {
			   ctx->node[sid].status = BUDDY_STATUS_PARTIAL;
			   if (lid != BUDDY_NODE_ID_NULL)
					ctx->node[lid].status = BUDDY_STATUS_USED_L;
					
			   if (i == 0)
			   {
					ctx->node[sid].bitmap |= BUDDY_BITMAP_ALLOC[0][4 - remain];
					add_to_slist(ctx, sid);
					break;
			   }
					
			   buddy_node_id_t left = BUDDY_LEFT_CHILD(sid, i);
			   buddy_node_id_t right = BUDDY_RIGHT_CHILD(sid, i);

			   ctx->node[left].status = BUDDY_STATUS_USED_R;
			   
			   if (remain < (1 << (i + 1)))
			   {
					sid = right;
					lid = left;

					-- i;
			   }
			   else
			   {
					add_to_clist(ctx, right, i - 1);
					remain -= (1 << (i + 1));
					sid = left;
					
					-- i;
			   }
		  }
		  
		  return BUDDY_LEFT_TAIL(cur, lev) << 1;
	 }
	 return BUDDY_NODE_ID_NULL;
}

static inline void
node_free(struct buddy_context_s *ctx, buddy_node_id_t node, int level)
{
	 ctx->node[node].status = BUDDY_STATUS_FREE;
	 // Merge free blocks
	 while (ctx->node[node].type != BUDDY_TYPE_ROOT)
	 {
		  buddy_node_id_t parent, sibling;
		  if (ctx->node[node].type == BUDDY_TYPE_LEFT)
		  {
			   parent = BUDDY_LEFT_PARENT(node, level);
			   sibling = BUDDY_RIGHT_CHILD(parent, level + 1);
		  }
		  else
		  {
			   parent = BUDDY_RIGHT_PARENT(node, level);
			   sibling = BUDDY_LEFT_CHILD(parent, level + 1);
		  }
		  
		  if (ctx->node[sibling].status == BUDDY_STATUS_FREE)
		  {
			   remove_from_clist(ctx, sibling, level);
			   ctx->node[parent].status = BUDDY_STATUS_FREE;
		  }
		  else break;
		  
		  node = parent;
		  ++ level;
	 }
	 add_to_clist(ctx, node, level);
}

int
buddy_free(struct buddy_context_s *ctx, buddy_node_id_t start)
{
	 int offset = start & 3;
	 buddy_node_id_t node = (start - offset) >> 1;
	 int result = 0;

	 if (ctx->node[node].status == BUDDY_STATUS_PARTIAL)
	 {
	 L0:
		  remove_from_slist(ctx, node);
		  int mask = BUDDY_BITMAP_FREE[ctx->node[node].bitmap][offset];
		  ctx->node[node].bitmap ^= mask;
		  if (ctx->node[node].bitmap == 0)
		  {
			   node_free(ctx, node, 0);
		  }
		  else
		  {
			   add_to_slist(ctx, node);
		  }

		  result += (BIT_SEARCH_LAST(mask >> (offset << 1)) >> 1) + 1;
	 }
	 else
	 {
		  int level = 0;
		  while (ctx->node[node].status != BUDDY_STATUS_USED_L &&
				 ctx->node[node].status != BUDDY_STATUS_USED_R)
		  {
			   node = BUDDY_LEFT_PARENT(node, level);
			   ++ level;
		  }

		  if (ctx->node[node].status == BUDDY_STATUS_USED_R)
		  {
			   node_free(ctx, node, level);
			   result = (1 << (level + 2));
		  }
		  else
		  {
			   node = BUDDY_LEFT_PARENT(node, level);
			   ++ level;
			   
			   while (1)
			   {
					if (level == 0)
					{
						 offset = 0;
						 goto L0;
					}
					
					buddy_node_id_t left  = BUDDY_LEFT_CHILD(node, level);
					buddy_node_id_t right = BUDDY_RIGHT_CHILD(node, level);
					
					if (ctx->node[left].status == BUDDY_STATUS_USED_R)
					{
						 node_free(ctx, left, level - 1);
						 result += (1 << (level + 1));
						 break;
					}
					if (ctx->node[left].status == BUDDY_STATUS_USED_L)
					{
						 node_free(ctx, left, level - 1);
						 node = right;
						 result += (1 << (level + 1));
						 -- level;
					}
					else
					{
						 node = left;
						 -- level;
					}
			   }
		  }
	 }
	 return result;
}

/* UGLY CODE FOR INITIALIZE THE CONSTS */
void
buddy_init(void)
{
	 if (BUDDY_INIT_COND) return;
	 BUDDY_INIT_COND = 0;

	 int u;
	 for (u = 0; u != 256; ++ u)
	 {
		  BUDDY_SN[u] = 0;
		  
		  int v;
		  int w;
		  int sn;
		  int fs;
		  int ul; 
		  int i;

		  v = u;
		  sn = 0;
		  fs = -1;
		  ul = 0;

		  for (i = 0; i != 4; ++ i)
		  {
			   BUDDY_BITMAP_FREE[u][i] = -1;
		  }
		  
		  for (i = 0; i != 4; ++ i)
		  {
			   int b = v & 3;
			   v >>= 2;

			   if (b == 0)
			   {
					ul = 0;
					++ sn;
			   }
			   else
			   {
					sn = 0;
					if (b == 2)
					{
						 ++ ul;
					}
					else if (b == 1)
					{
						 int j;
						 w = 0;
						 for (j = i; j >= i - ul; -- j)
						 {
							  if (w == 0) w = 1;
							  else w = (w << 2) | 2;
							  BUDDY_BITMAP_FREE[u][j] = w << (j << 1);
						 }
							  
						 ul = 0;
					}
					else ul = 0;
			   }

			   if (sn > BUDDY_SN[u])
			   {
					BUDDY_SN[u] = sn;
					fs = i - sn + 1;
			   }
		  }

		  w = 0;
		  for (i = 0; i != 4; ++ i)
		  {
			   if (i > BUDDY_SN[u]) BUDDY_BITMAP_ALLOC[u][i] = -1;
			   else BUDDY_BITMAP_ALLOC[u][i] = w << (fs << 1);

			   if (w == 0) w = 1;
			   else w = (w << 2) | 2;
		  }
	 }
}
