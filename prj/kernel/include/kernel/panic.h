#ifndef __KERN_PANIC_H__
#define __KERN_PANIC_H__

#include <lib/low_io.h>

#define panic(x ...) { cprintf("PANIC: " x); while (1) ;}

#endif
