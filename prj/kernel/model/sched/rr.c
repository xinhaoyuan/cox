#include <sched/rr.h>

static void
rr_init(runqueue_t rq) {
	rq->rr.first = NULL;
}

static void
rr_enqueue(runqueue_t rq, sched_node_t node)
{
	if (rq->rr.first == NULL)
	{
		rq->rr.first = node;
		node->rr.prev = node->rr.next = node;
	}
	else
	{
		node->rr.next = rq->rr.first;
		node->rr.prev = node->rr.next->rr.prev;
		
		node->rr.next->rr.prev = node;
		node->rr.prev->rr.next = node;
	}
}

static void
rr_dequeue(runqueue_t rq, sched_node_t node)
{
	if (node->rr.next == node)
	{
		rq->rr.first = NULL;
	}
	else
	{
		node->rr.next->rr.prev = node->rr.prev;
		node->rr.prev->rr.next = node->rr.next;

		if (rq->rr.first == node)
			rq->rr.first = node->rr.next;
	}
}

static sched_node_t
rr_pick(runqueue_t rq) {
	return rq->rr.first;
}

sched_class_s sched_class_rr = 
{
	.init = rr_init,
	.enqueue = rr_enqueue,
	.dequeue = rr_dequeue,
	.pick = rr_pick,
};
