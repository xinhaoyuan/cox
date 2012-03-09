#include <types.h>
#include <timer.h>
#include <proc.h>
#include <malloc.h>
#include <page.h>
#include <string.h>
#include <arch/cc.h>
#include <arch/sys_arch.h>
#include <arch/mmu.h>
#include <arch/memlayout.h>
#include <lwip/sys.h>
#include <lwip/err.h>
#include <lwipopts.h>

#define TICK_FREQ 100

#define USE_HPET 0

static volatile proc_t     prot_proc;
static volatile sys_prot_t prot_lev;
static mutex_s prot_mutex;

struct sys_timeouts {
	struct sys_timeo *next;
};

struct sys_thread_s
{
	void (*func)(void *arg);
	void *arg;
	 
	proc_t proc;
	struct sys_timeouts timeouts;
};

void
sys_init(void)
{
	struct sys_thread_s *w = (struct sys_thread_s *)
		kmalloc(sizeof(struct sys_thread_s));
	w->func = NULL;
	w->arg  = NULL;
	w->proc = current;
	memset(&w->timeouts, 0, sizeof(w->timeouts));

	w->proc->priv = w;
	 
	mutex_init(&prot_mutex);
	prot_lev = 0;
}

err_t
sys_sem_new(sys_sem_t *sem, u8_t count)
{
	sem->valid = 1;
	semaphore_init(&sem->sem, count);
#if LWIP_DEBUG
	// ekf_kprintf("%d %u sys_sem_new %x %d\n", proc_self(), *hpet_tick, result, count);
#endif
	return 0;
}

void
sys_sem_free(sys_sem_t *sem)
{
	sem->valid = 0;
}

int
sys_sem_valid(sys_sem_t *sem)
{
	return sem->valid;
}

void
sys_sem_set_invalid(sys_sem_t *sem)
{
	sem->valid = 0;
}

void
sys_sem_signal(sys_sem_t *sem)
{
#if LWIP_DEBUG
	// ekf_kprintf("%d %u sys_sem_signal %x\n", proc_self(), *hpet_tick, sem);
#endif
	semaphore_release(&sem->sem);
}

u32_t
sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
#if USE_HPET
	u32_t tick_old = *hpet_tick;
#else
	tick_t tick_old = timer_tick;
#endif
	u32_t result;
#if LWIP_DEBUG
	/* if (timeout > 500) */
	// cprintf("sys_arch_sem_wait\n");
	cprintf("sys_arch_sem_wait %016lx %d\n", sem, timeout);
#endif
	 
	ips_node_s node;
//	 ekf_memset(&node, 0, sizeof(node));
	if (timeout == 0)
	{
		semaphore_acquire(&sem->sem, NULL);
#if USE_HPET
		result = (uint32_t)((*hpet_tick - tick_old) * 1000.0 / hpet_tick_freq);
#else
		result = (uint32_t)((timer_tick - tick_old) * 1000.0 / TICK_FREQ);
#endif
		  
	}
	else
	{
		tick_t sleep = TICK_FREQ * 0.001 * timeout;
		int dsn_succ = !proc_delayed_self_notify_set(sleep);
		if (semaphore_acquire(&sem->sem, &node))
		{
			 
			while (1)
			{
				if (dsn_succ && !proc_dsn_active())
				{
					semaphore_ac_break(&sem->sem, &node);
				}

				if (ips_wait_try(&node) == 0) break;
				proc_wait_try();
			}
		}

		/* The magic number came from ../include/lwip/sys.h */
		if (ips_is_finish(&node))
#if USE_HPET
			result = (uint32_t)((*hpet_tick - tick_old) * 1000.0 / hpet_tick_freq);
#else
		result = (uint32_t)((timer_tick - tick_old) * 1000.0 / TICK_FREQ);
#endif
		else return SYS_ARCH_TIMEOUT;
	}
	// ekf_kprintf("sem acquire r = %d, timeout = %d\n", result, timeout);
	return result; // > timeout ? timeout : result;
}

err_t
sys_mbox_new(sys_mbox_t *mbox, int size)
{
#if LWIP_DEBUG
	// ekf_kprintf("%d %u sys_mbox_new %d\n", proc_self(), *hpet_tick, size);
#endif
	if (size < 4)
		size = 512;
	mbox->size = size;
	mbox->head = 0;
	mbox->tail = 0;
	if ((mbox->entry = (void **)kmalloc(sizeof(void *) * size)) == NULL)
	{
#ifndef LWIP_DEBUG
		cprintf("sys mbox new failed\n");
#endif
		return -ERR_MEM;
	}
	mutex_init(&mbox->lock);
	semaphore_init(&mbox->recv_sem, 0);
	semaphore_init(&mbox->send_sem, size - 1);

	mbox->valid = 1;
	return 0;
}

void
sys_mbox_free(sys_mbox_t *mbox)
{
	mbox->valid = 0;
}

int
sys_mbox_valid(sys_mbox_t *mbox)
{
	return mbox->valid;
}

void
sys_mbox_set_invalid(sys_mbox_t *mbox)
{
	mbox->valid = 0;
}

void
sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
#if LWIP_DEBUG
	cprintf("sys_arch_mbox_post %016lx\n", mbox);
	// ekf_kprintf("%d %u sys_mbox_post %x\n", proc_self(), *hpet_tick, mbox);
#endif
	ips_node_s node;
//	 ekf_memset(&node, 0, sizeof(node));
	if (semaphore_acquire(&mbox->send_sem, &node))
		ips_wait(&node);

	mutex_acquire(&mbox->lock, NULL);
	int slot = mbox->head;
	mbox->head = (mbox->head + 1) % mbox->size;
	mbox->entry[slot] = msg;
	mutex_release(&mbox->lock);

	semaphore_release(&mbox->recv_sem);
}

err_t
sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
#if LWIP_DEBUG
	// ekf_kprintf("%d %u sys_mbox_trypost %x\n", proc_self(), *hpet_tick, mbox);
#endif
	if (semaphore_try_acquire(&mbox->send_sem) == 0)
	{
		mutex_acquire(&mbox->lock, NULL);
		int slot = mbox->head;
		mbox->head = (mbox->head + 1) % mbox->size;
		mbox->entry[slot] = msg;
		mutex_release(&mbox->lock);

		semaphore_release(&mbox->recv_sem);
		return 0;
	}
	else
	{
		return ERR_MEM;
	}
}

u32_t
sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
#if USE_HPET
	u32_t tick_old = *hpet_tick;
#else
	tick_t tick_old = timer_tick;
#endif
	ips_node_s node;
	 
	if (timeout == 0)
	{
		semaphore_acquire(&mbox->recv_sem, NULL);
	}
	else
	{
		int dsn_succ = !proc_delayed_self_notify_set(TICK_FREQ * 0.001 * timeout);
		 
		// ekf_kprintf("[%d]mbox open timer %d for timeout %d\n", proc_self(), t, timeout);
		if (semaphore_acquire(&mbox->recv_sem, &node))
		{
			while (1)
			{
				if (dsn_succ && !proc_dsn_active())
				{
					semaphore_ac_break(&mbox->recv_sem, &node);
				}
				if (ips_wait_try(&node) == 0) break;
				proc_wait_try();
			}
		}
		 
		if (!ips_is_finish(&node))
		{
			// uint32_t r = (uint32_t)((*hpet_tick - hpet_tick_old) * 1000.0 / hpet_tick_freq);
			// ekf_kprintf("[%d]mbox fetch failed, r = %d, timeout = %d\n", proc_self(), r, timeout);
			return SYS_ARCH_TIMEOUT;
		}
	}
	 
	mutex_acquire(&mbox->lock, NULL);
	int slot = mbox->tail;
	mbox->tail = (mbox->tail + 1) % mbox->size;
	*msg = mbox->entry[slot];
	mutex_release(&mbox->lock);
	 
	semaphore_release(&mbox->send_sem);
	uint32_t r;
#if USE_HPET
	r = (uint32_t)((*hpet_tick - tick_old) * 1000.0 / hpet_tick_freq);
#else
	r = (uint32_t)((timer_tick - tick_old) * 1000.0 / TICK_FREQ);
#endif

	// ekf_kprintf("[%d]mbox fetch r = %d, timeout = %d\n", proc_self(), r, timeout);
	// if (r > timeout + 50 && timeout > 0) ekf_kprintf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! mbox\n");
	return r; // > timeout ? timeout : r;
}

u32_t
sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
	if (semaphore_try_acquire(&mbox->recv_sem) == 0)
	{
		mutex_acquire(&mbox->lock, NULL);
		int slot = mbox->tail;
		mbox->tail = (mbox->tail + 1) % mbox->size;
		*msg = mbox->entry[slot];
		mutex_release(&mbox->lock);

		semaphore_release(&mbox->send_sem);
		return 0;
	}
	else
	{
		return SYS_MBOX_EMPTY;
	}
}

struct sys_timeouts *
sys_arch_timeouts(void)
{
	if (current->priv == NULL)
		return NULL;
	 
	struct sys_thread_s *w = (struct sys_thread_s *)current->priv;
	return &w->timeouts;
}

void
_wrapper_thread(void *_w)
{
	current->priv = _w;
	struct sys_thread_s *w = (struct sys_thread_s *)_w;
	w->func(w->arg);
}

sys_thread_t
sys_thread_new(const char *name, void (*func)(void *arg), void *arg, int stacksize, int prio)
{
	proc_t proc = (proc_t)kmalloc(sizeof(proc_s));
	struct sys_thread_s *w = (struct sys_thread_s *)kmalloc(sizeof(struct sys_thread_s));
	w->func = func;
	w->arg = arg;
	w->proc = proc;
	memset(&w->timeouts, 0, sizeof(struct sys_timeouts));
	
	if (stacksize < 4 * PGSIZE) stacksize = 4 * PGSIZE;
	stacksize = (stacksize + PGSIZE - 1) & ~(PGSIZE - 1);
	void *stack = kmalloc(stacksize);

	proc_init(proc, name, SCHED_CLASS_RR, &_wrapper_thread, w, (uintptr_t)ARCH_STACKTOP(stack, stacksize));
	proc_notify(proc);
	return w;
}

sys_prot_t
sys_arch_protect(void)
{
#if LWIP_DEBUG
	// ekf_kprintf("%d %u sys_arch_protect %d\n", proc_self(), *hpet_tick, prot_lev);
#endif
	if (mutex_try_acquire(&prot_mutex) != 0)
	{
		if (prot_proc != current)
		{
//			   ekf_memset(&node, 0, sizeof(node));
#if LWIP_DEBUG
			// cprintf("%p %u sys_arch_protect wait mutex\n", current, 0);
#endif			   
			mutex_acquire(&prot_mutex, NULL);
#if LWIP_DEBUG
			// cprintf("%p %u sys_arch_protect get mutex %d\n", current, 0, prot_lev);
#endif			   

		}
	}

	int result = prot_lev ++;
#if LWIP_DEBUG
	// ekf_kprintf("%d %u sys_arch_protect exit %d\n", proc_self(), *hpet_tick, result);
#endif

	prot_proc = current;
	return result;
}

void
sys_arch_unprotect(sys_prot_t pval)
{
#if LWIP_DEBUG
	// ekf_kprintf("%d %u sys_arch_unprotect %d\n", proc_self(), *hpet_tick, pval);
#endif
	if ((prot_lev = pval) == 0)
	{
		prot_proc = NULL;
		mutex_release(&prot_mutex);
	}
}
