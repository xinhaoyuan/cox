#ifndef __E1000_GLUE_H__
#define __E1000_GLUE_H__

#include <types.h>

typedef struct ether_addr
{
	u8_t ea_addr[6];
} ether_addr_t;

void e1000_test(void);

#endif
