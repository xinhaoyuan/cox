#ifndef __KERN_PAGEBUF_H__
#define __KERN_PAGEBUF_H__

#include <types.h>
#include <spinlock.h>

struct pagebuf_s
{
	spinlock_s lock;
	int size;
	int count;
	uintptr_t buf[0];
};

#define PAGEBUF_SIZE(AREA_SIZE) (((AREA_SIZE) - sizeof(struct pagebuf_s)) / sizeof(uintptr_t))

typedef struct pagebuf_s pagebuf_s;
typedef pagebuf_s *pagebuf_t;

void pagebuf_init(pagebuf_t buf, int size);
int  pagebuf_alloc(pagebuf_t buf, int num, uintptr_t *addr);
void pagebuf_free(pagebuf_t buf, int num, uintptr_t *addr);

#endif
