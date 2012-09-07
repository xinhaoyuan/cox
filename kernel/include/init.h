#ifndef __KERN_INIT_H__
#define __KERN_INIT_H__

void kern_sys_init_global(void);
void kern_sys_init_local(void);
void kern_init(void);

void do_idle(void) __attribute__((noreturn));

#endif
