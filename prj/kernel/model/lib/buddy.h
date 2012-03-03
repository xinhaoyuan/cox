#ifndef __BUDDY_H__
#define __BUDDY_H__

#include <types.h>

#define BUDDY_CLEVEL_COUNT_MAX 52
typedef uint64_t buddy_node_id_t;
#define BUDDY_NODE_ID_NULL ((buddy_node_id_t)-1)

struct buddy_node_s
{
	 uint32_t type   : 8;
	 uint32_t status : 8;
	 uint32_t bitmap : 8;
	 buddy_node_id_t next   : BUDDY_CLEVEL_COUNT_MAX;
	 buddy_node_id_t prev   : BUDDY_CLEVEL_COUNT_MAX;
} __attribute__((packed));

struct buddy_context_s
{
	 /* Should be initialized outside before buddy_build {{{ */
	 struct buddy_node_s *node;
	 /* }}} */
	 
	 buddy_node_id_t clist[BUDDY_CLEVEL_COUNT_MAX];
	 buddy_node_id_t slist[4];

	 uint64_t clist_bm;
	 uint32_t slist_bm;
	 
	 buddy_node_id_t node_count;
	 int clist_count;
};

#define BUDDY_CALC_ARRAY_SIZE(num)  (((((num) - 1) | 3) + 1) << 2)
#define BUDDY_CALC_CLIST_COUNT(num) (BIT_SEARCH_LAST(num) > 1 ? (BIT_SEARCH_LAST(num) - 1) : 0)
#define BUDDY_CALC_NODE_COUNT(num)  ((((((num) - 1) | 3) + 1) >> 1) - 1)

typedef int(*buddy_initial_is_free_f)(buddy_node_id_t num);

void            buddy_init(void);
int             buddy_build(struct buddy_context_s *ctx, buddy_node_id_t num, buddy_initial_is_free_f config);
buddy_node_id_t buddy_alloc(struct buddy_context_s *ctx, buddy_node_id_t num);
/* Return the block size */
int             buddy_free(struct buddy_context_s *ctx,  buddy_node_id_t start);

#endif
