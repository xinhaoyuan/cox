#ifndef __KERN_ARCH_PIC_H__
#define __KERN_ARCH_PIC_H__

int  pic_init(void);
void pic_enable(unsigned int irq);

#endif

