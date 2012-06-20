#include "idle.h"

static void
idle_init(runqueue_t rq)
{
	rq->idle.node = NULL;
}

static void
idle_enqueue(runqueue_t rq, sched_node_t node) { }

static void
idle_dequeue(runqueue_t rq, sched_node_t node) { }

static sched_node_t
idle_pick(runqueue_t rq)
{
	return rq->idle.node;
}

sched_class_s sched_class_idle = 
{
	.init = idle_init,
	.enqueue = idle_enqueue,
	.dequeue = idle_dequeue,
	.pick = idle_pick,
};
