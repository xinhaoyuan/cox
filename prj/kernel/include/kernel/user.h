#ifndef __KERN_USER_H__
#define __KERN_USER_H__

#include <types.h>
#include <proc.h>
#include <user/io.h>
#include <arch/user.h>

struct user_thread_s
{
	void     *tls;
	uintptr_t tls_u;
	
	uintptr_t iobuf_size;
	uintptr_t iocb_stack_size;
	uintptr_t user_size;
	
	struct
	{
		iobuf_index_t cap;
		io_call_entry_t head;
	} ioce;
	
	struct
	{
		iobuf_index_t  cap;
		iobuf_index_t *entry;
		uintptr_t     *head, *tail, *busy;
		void          *stack;
		uintptr_t      stack_u;
		uintptr_t      callback_u;
	} iocb;

	user_thread_arch_s arch;
};

typedef struct user_thread_s  user_thread_s;
typedef struct user_thread_s *user_thread_t;

void user_thread_jump(void) __attribute__((noreturn));
/* filled by arch */
int user_thread_arch_push_iocb(void);
int user_thread_arch_init(proc_t proc, uintptr_t entry);
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

int user_mm_copy_page(user_mm_t mm, uintptr_t addr, uintptr_t phys, int flag);
int user_mm_copy(user_mm_t mm, uintptr_t addr, void *src, size_t size);

/* filled by arch */
int user_mm_arch_init(user_mm_t mm, uintptr_t *start, uintptr_t *end);
int user_mm_arch_copy_page(user_mm_t mm, uintptr_t addr, uintptr_t phys, int flag);
int user_mm_arch_copy(user_mm_t mm, uintptr_t addr, void *src, size_t size);
void *user_mm_arch_memmap_open(user_mm_t mm, uintptr_t addr, size_t size);
void  user_mm_arch_memmap_close(user_mm_t mm, void *addr);

void user_process_io(proc_t proc);
void user_before_return(proc_t proc);
void user_save_context(proc_t proc);
void user_restore_context(proc_t proc);

void user_arch_save_context(proc_t proc);
void user_arch_restore_context(proc_t proc);

int user_proc_load(void *bin, size_t bin_size);

#endif
