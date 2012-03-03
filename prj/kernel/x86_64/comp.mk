CC :=	 gcc -m64 -ffreestanding \
		-mcmodel=kernel -mno-red-zone \
		-mno-mmx -mno-sse -mno-sse2 \
		-mno-sse3 -mno-3dnow \
		-fno-builtin -fno-builtin-function -nostdinc -fno-stack-protector

LD		:= ld -m $(shell ld -V | grep elf_x86_64 2>/dev/null) -nostdlib
OBJCOPY := objcopy
