#ifndef __KERN_ARCH_PAGING_H__
#define __KERN_ARCH_PAGING_H__

#define PGFLT_USER(err)    (!!((err) & 4))
#define PGFLT_WRITE(err)   (!!((err) & 2))
#define PGFLT_READ(err)    (! ((err) & 2))
#define PGFLT_PRESENT(err) (!!((err) & 1))

#endif
