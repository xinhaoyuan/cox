#include <types.h>
#include <arch/cc.h>
#include <arch/sys_arch.h>

#define LWIP_DEBUG
#define USE_HPET 0

/* from ../include/lwip/sys.h */
#define SYS_ARCH_TIMEOUT 0xffffffffUL
#define SYS_MBOX_EMPTY   SYS_ARCH_TIMEOUT

typedef char err_t;
/* from ../include/lwip/err.h */
#define ERR_OK          0    /* No error, everything OK. */
#define ERR_MEM        -1    /* Out of memory error.     */
#define ERR_BUF        -2    /* Buffer error.            */
#define ERR_TIMEOUT    -3    /* Timeout.                 */
#define ERR_RTE        -4    /* Routing problem.         */
#define ERR_INPROGRESS -5    /* Operation in progress    */
#define ERR_VAL        -6    /* Illegal value.           */
#define ERR_WOULDBLOCK -7    /* Operation would block.   */
#define ERR_USE        -8    /* Address in use.          */
#define ERR_ISCONN     -9    /* Already connected.       */

#define ERR_IS_FATAL(e) ((e) < ERR_ISCONN)

#define ERR_ABRT       -10   /* Connection aborted.      */
#define ERR_RST        -11   /* Connection reset.        */
#define ERR_CLSD       -12   /* Connection closed.       */
#define ERR_CONN       -13   /* Not connected.           */

#define ERR_ARG        -14   /* Illegal argument.        */

#define ERR_IF         -15   /* Low-level netif error    */

static volatile proc_t     prot_proc;
static volatile sys_prot_t prot_lev;
static mutex_t prot_mutex;

/* definition from ../include/lwip/sys.h */
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
		  ekf_alloc(sizeof(struct sys_thread_s));
	 w->func = NULL;
	 w->arg  = NULL;
	 w->proc = proc_self();
	 memset(&w->timeouts, 0, sizeof(w->timeouts));

	 proc_priv_set(w);
	 
	 mutex_init(&prot_mutex);
	 prot_lev = 0;
}

err_t
sys_sem_new(sys_sem_t *sem, u8_t count)
{
	 sem->valid = 1;
	 semaphore_init(&sem->sem, count);
#ifdef LWIP_DEBUG
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
#ifdef LWIP_DEBUG
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
	 u32_t tick_old = timer_tick_get();
#endif
	 u32_t result;
#ifdef LWIP_DEBUG
	 /* if (timeout > 500) */
	 /* 	  ekf_kprintf("%d %u sys_arch_sem_wait %x %d\n", proc_self(), *hpet_tick, sem, timeout); */
#endif
	 
	 ips_node_t node;
//	 ekf_memset(&node, 0, sizeof(node));
	 if (timeout == 0)
	 {
		  semaphore_acquire(&sem->sem, NULL, NULL);
#if USE_HPET
		  result = (uint32_t)((*hpet_tick - tick_old) * 1000.0 / hpet_tick_freq);
#else
		  result = (uint32_t)((timer_tick_get() - tick_old) * 1000.0 / timer_freq_get());
#endif
		  
	 }
	 else
	 {
		  proc_legacy_wait_pretend();
		  
		  timer_t t = timer_open((timer_cb_f)proc_legacy_notify, (void *)proc_self(), 0, timer_tick_get() + timer_freq_get() * 0.001 * timeout);
		  if (semaphore_acquire(&sem->sem, &node, NULL))
		  {

			   while (1)
			   {
					if (timer_is_closed(t))
					{
						 semaphore_ac_break(&sem->sem, &node);
					}
					if (ips_wait_try(&node) == 0) break;
					proc_legacy_wait_try();
			   }
		  }

		  timer_close(t);
			   
		  /* The magic number came from ../include/lwip/sys.h */
		  if (ips_is_finish(&node))
#if USE_HPET
			   result = (uint32_t)((*hpet_tick - tick_old) * 1000.0 / hpet_tick_freq);
#else
			   result = (uint32_t)((timer_tick_get() - tick_old) * 1000.0 / timer_freq_get());
#endif
		  else return SYS_ARCH_TIMEOUT;
	 }
	 // ekf_kprintf("sem acquire r = %d, timeout = %d\n", result, timeout);
	 return result; // > timeout ? timeout : result;
}

err_t
sys_mbox_new(sys_mbox_t *mbox, int size)
{
#ifdef LWIP_DEBUG
	 // ekf_kprintf("%d %u sys_mbox_new %d\n", proc_self(), *hpet_tick, size);
#endif
	if (size < 4)
		size = 512;
	 mbox->size = size;
	 mbox->head = 0;
	 mbox->tail = 0;
	 if ((mbox->entry = (void **)ekf_alloc(sizeof(void *) * size)) == 0)
		  return -ERR_MEM;
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
#ifdef LWIP_DEBUG
	 // ekf_kprintf("%d %u sys_mbox_post %x\n", proc_self(), *hpet_tick, mbox);
#endif
	 ips_node_t node;
//	 ekf_memset(&node, 0, sizeof(node));
	 if (semaphore_acquire(&mbox->send_sem, &node, NULL))
		  ips_wait(&node);

	 mutex_acquire(&mbox->lock, NULL, NULL);
	 int slot = mbox->head;
	 mbox->head = (mbox->head + 1) % mbox->size;
	 mbox->entry[slot] = msg;
	 mutex_release(&mbox->lock);

	 semaphore_release(&mbox->recv_sem);
}

err_t
sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
#ifdef LWIP_DEBUG
	 // ekf_kprintf("%d %u sys_mbox_trypost %x\n", proc_self(), *hpet_tick, mbox);
#endif
	 if (semaphore_try_acquire(&mbox->send_sem) == 0)
	 {
		  mutex_acquire(&mbox->lock, NULL, NULL);
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
	 u32_t tick_old = timer_tick_get();
#endif
	 ips_node_t node;
	 
	 if (timeout == 0)
	 {
		  semaphore_acquire(&mbox->recv_sem, NULL, NULL);
	 }
	 else
	 {
		  proc_legacy_wait_pretend();
		  
		  timer_t t = timer_open((timer_cb_f)proc_legacy_notify, (void *)proc_self(), 0, timer_tick_get() + timer_freq_get() * 0.001 * timeout);

		  // ekf_kprintf("[%d]mbox open timer %d for timeout %d\n", proc_self(), t, timeout);
		  if (semaphore_acquire(&mbox->recv_sem, &node, NULL))
		  {
			   while (1)
			   {
					if (timer_is_closed(t))
					{
						 semaphore_ac_break(&mbox->recv_sem, &node);
					}
					if (ips_wait_try(&node) == 0) break;
					proc_legacy_wait_try();
			   }
		  }

		  timer_close(t);
		  
		  if (!ips_is_finish(&node))
		  {
			   // uint32_t r = (uint32_t)((*hpet_tick - hpet_tick_old) * 1000.0 / hpet_tick_freq);
			   // ekf_kprintf("[%d]mbox fetch failed, r = %d, timeout = %d\n", proc_self(), r, timeout);
			   return SYS_ARCH_TIMEOUT;
		  }
	 }

	 mutex_acquire(&mbox->lock, NULL, NULL);
	 int slot = mbox->tail;
	 mbox->tail = (mbox->tail + 1) % mbox->size;
	 *msg = mbox->entry[slot];
	 mutex_release(&mbox->lock);

	 semaphore_release(&mbox->send_sem);
	 uint32_t r;
#if USE_HPET
	 r = (uint32_t)((*hpet_tick - tick_old) * 1000.0 / hpet_tick_freq);
#else
	 r = (uint32_t)((timer_tick_get() - tick_old) * 1000.0 / timer_freq_get());
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
		  mutex_acquire(&mbox->lock, NULL, NULL);
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
	 if (proc_self_type_get() != PROC_TYPE_LWIP)
		  return NULL;
	 
	 struct sys_thread_s *w = (struct sys_thread_s *)proc_priv_get();
	 return &w->timeouts;
}

void
_wrapper_thread(void *_w)
{
	 proc_self_type_set(PROC_TYPE_LWIP);
	 proc_priv_set(_w);
	 struct sys_thread_s *w = (struct sys_thread_s *)_w;
	 w->func(w->arg);
}

sys_thread_t
sys_thread_new(const char *name, void (*func)(void *arg), void *arg, int stacksize, int prio)
{
	 struct sys_thread_s *w = (struct sys_thread_s *)ekf_alloc(sizeof(struct sys_thread_s));
	 w->func = func;
	 w->arg = arg;
	 memset(&w->timeouts, 0, sizeof(struct sys_timeouts));
	 
	 if (stacksize < 8192) stacksize = 8192;
	 proc_attach(w->proc = proc_create(name, 0, stacksize, proc_kvpt_get(), &_wrapper_thread, w));
	 return w;
}

sys_prot_t
sys_arch_protect(void)
{
#ifdef LWIP_DEBUG
	 // ekf_kprintf("%d %u sys_arch_protect %d\n", proc_self(), *hpet_tick, prot_lev);
#endif
	 if (mutex_try_acquire(&prot_mutex) != 0)
	 {
		  if (prot_proc != proc_self())
		  {
//			   ekf_memset(&node, 0, sizeof(node));
#ifdef LWIP_DEBUG
			   // ekf_kprintf("%d %u sys_arch_protect wait mutex\n", proc_self(), *hpet_tick);
#endif			   
			   mutex_acquire(&prot_mutex, NULL, NULL);
#ifdef LWIP_DEBUG
			   // ekf_kprintf("%d %u sys_arch_protect get mutex %d\n", proc_self(), *hpet_tick, prot_lev);
#endif			   

		  }
	 }

	 int result = prot_lev ++;
#ifdef LWIP_DEBUG
	 // ekf_kprintf("%d %u sys_arch_protect exit %d\n", proc_self(), *hpet_tick, result);
#endif

	 prot_proc = proc_self();
	 return result;
}

void
sys_arch_unprotect(sys_prot_t pval)
{
#ifdef LWIP_DEBUG
	 // ekf_kprintf("%d %u sys_arch_unprotect %d\n", proc_self(), *hpet_tick, pval);
#endif
	 if ((prot_lev = pval) == 0)
	 {
		  prot_proc = NULL;
		  mutex_release(&prot_mutex);
	 }
}
