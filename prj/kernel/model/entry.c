#include <cpu.h>
#include <entry.h>

void
model_entry(void)
{
	while (1) __cpu_relax();
}
