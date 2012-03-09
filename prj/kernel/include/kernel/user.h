#ifndef __KERN_USER_H__
#define __KERN_USER_H__

#include <types.h>
#include <proc.h>
#include <user/io.h>
#include <arch/user.h>

struct user_thread_s
{
	struct
	{
		iobuf_index_t cap;
		io_call_entry_t head;
	} ioce;
	
	struct
	{
		iobuf_index_t  cap;
		iobuf_index_t *entry;
		uintptr_t *head, *tail;
		uintptr_t *busy;
		uintptr_t  stack;
		size_t     stack_size;
		io_callback_handler_f callback;
	} iocb;

	user_thread_arch_s arch;
};

typedef struct user_thread_s  user_thread_s;
typedef struct user_thread_s *user_thread_t;

int user_thread_init(void);
void user_thread_fill(uintptr_t cb, size_t cb_size, uintptr_t iobuf, size_t iobuf_size, uintptr_t entry, uintptr_t stacktop);
void user_thread_jump(void) __attribute__((noreturn));
/* filled by arch */
int user_thread_arch_push_iocb(void);
int user_thread_arch_init(void);
void user_thread_arch_fill(uintptr_t entry, uintptr_t stacktop);
int user_thread_arch_in_cb_stack(void);
void user_thread_arch_jump(void) __attribute__((noreturn));

struct user_mm_s
{
	/* address range */
	uintptr_t start;
	uintptr_t end;

	/* arch data */
	user_mm_arch_s arch;
};

typedef struct user_mm_s  user_mm_s;
typedef struct user_mm_s *user_mm_t;

int user_mm_init(user_mm_t mm, uintptr_t *start, uintptr_t *end);
int user_mm_copy_page(user_mm_t mm, uintptr_t addr, uintptr_t phys, int flag);
int user_mm_copy(user_mm_t mm, uintptr_t addr, void *src, size_t size);

/* filled by arch */
int user_mm_arch_init(user_mm_t mm, uintptr_t *start, uintptr_t *end);
int user_mm_arch_copy_page(user_mm_t mm, uintptr_t addr, uintptr_t phys, int flag);
int user_mm_arch_copy(user_mm_t mm, uintptr_t addr, void *src, size_t size);

void user_before_return(proc_t proc);
void user_arch_before_return(proc_t proc);

int user_proc_load(void *bin, size_t bin_size);

#endif
