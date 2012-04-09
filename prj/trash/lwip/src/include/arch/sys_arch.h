#ifndef __EKOS_LWIP_SYS_ARCH_H__
#define __EKOS_LWIP_SYS_ARCH_H__

#include <ips.h>
#include "cc.h"

typedef struct sys_sem_s {
	 int         valid;
	 semaphore_s sem;
} sys_sem_t;

typedef struct sys_mbox_s
{
	 int valid;
	 
	 unsigned int size;
	 unsigned int head;
	 unsigned int tail;
	 void       **entry;
	 
	 mutex_s     lock;
	 semaphore_s recv_sem;
	 semaphore_s send_sem;
} sys_mbox_t;

typedef unsigned int sys_prot_t;

typedef struct sys_thread_s *sys_thread_t;

void sys_init(void);

char sys_sem_new(sys_sem_t *sem, u8_t count);
void  sys_sem_free(sys_sem_t *sem);
int   sys_sem_valid(sys_sem_t *sem);
void  sys_sem_set_invalid(sys_sem_t *sem);

void  sys_sem_signal(sys_sem_t *sem);
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout);

char  sys_mbox_new(sys_mbox_t *mbox, int size);
void  sys_mbox_free(sys_mbox_t *mbox);
int   sys_mbox_valid(sys_mbox_t *mbox);
void  sys_mbox_set_invalid(sys_mbox_t *mbox);

void  sys_mbox_post(sys_mbox_t *mbox, void *msg);
char  sys_mbox_trypost(sys_mbox_t *mbox, void *msg);
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout);
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg);

struct sys_timeouts *sys_arch_timeouts(void);
sys_thread_t sys_thread_new(const char *name, void (* thread)(void *arg), void *arg, int stacksize, int prio);
sys_prot_t sys_arch_protect(void);
void sys_arch_unprotect(sys_prot_t pval);

#endif
