CC      := ${GCC_PREFIX}gcc -m32 -fno-builtin -fno-builtin-function -nostdinc -fno-stack-protector
LD		:= ${GCC_PREFIX}ld -m $(shell ld -V | grep elf_i386 2>/dev/null)
OBJDUMP	:= ${GCC_PREFIX}objdump
OBJCOPY := ${GCC_PREFIX}objcopy
STRIP   := ${GCC_PREFIX}strip
