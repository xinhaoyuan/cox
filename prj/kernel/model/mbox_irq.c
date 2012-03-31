#include <mbox_irq.h>
#include <mbox.h>
#include <irq.h>
#include <lib/low_io.h>

static int mbox_irq[IRQ_COUNT];
static struct mbox_irq_data_s
{
	int irq_id;
	size_t busy_chain;
} mbox_irq_data[IRQ_COUNT];

static volatile int mbox_irq_initialized = 0;

int
mbox_irq_init(void)
{
	int i;
	for (i = 0; i < IRQ_COUNT; ++ i)
	{
		mbox_irq[i] = mbox_alloc(NULL);
		mbox_irq_data[i].irq_id = i;
		mbox_irq_data[i].busy_chain = 0;
	}

	mbox_irq_initialized = 1;	
	return 0;
}

int
mbox_irq_get(int irq_id)
{
	if (mbox_irq_initialized) return mbox_irq[irq_id];
	else return -1;
}

/* static void */
/* mbox_irq_ack(mbox_io_t io, void *__data, uintptr_t hint_a, uintptr_t hint_b) */
/* { */
/* 	struct mbox_irq_data_s *data = __data; */

/* 	if (hint_a == 0) */
/* 	{ */
/* 		data->busy_chain = 0; */
/* 		// irq_arch_ack(data->irq_id); */
/* 	} */
/* 	else if (-- data->busy_chain > 0) */
/* 	{ */
/* 		size_t foo; */
/* 		mbox_io_t next_io = mbox_io_try_acquire(mbox_irq[data->irq_id], &foo); */
/* 		if (next_io != NULL) */
/* 			mbox_io_send(io, mbox_irq_ack, __data, 0, 0); */
/* 		else */
/* 		{ */
/* 			/\* should not happen *\/ */
/* 			data->busy_chain = 0; */
/* 			// irq_arch_ack(data->irq_id); */
/* 		} */
/* 	} */
/* 	// else irq_arch_ack(data->irq_id); */
/* } */

int
mbox_irq_handler(int irq_id, uint64_t acc)
{
	/* if (!mbox_irq_initialized) return -1; */
	
	/* int mbox = mbox_irq[irq_id]; */
	/* if (mbox_irq_data[irq_id].busy_chain != 0) */
	/* 	return -1; */

	/* mbox_io_t io; */
	/* if ((io = mbox_io_try_acquire(mbox, &mbox_irq_data[irq_id].busy_chain)) == NULL) */
	/* 	return -1; */

	/* mbox_io_send(io, mbox_irq_ack, NULL, 0, 0); */
	return 0;
}
