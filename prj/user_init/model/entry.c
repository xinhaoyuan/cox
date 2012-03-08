#include <string.h>
#include <user/info.h>
#include <user/syscall.h>

struct startup_info_s info;

void
entry(struct startup_info_s __info)
{
	// memmove(&info, &__info, sizeof(info));
	__io();
	while (1) ;
}
