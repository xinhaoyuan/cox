.PHONY: all

CODENAME := $(shell cd ..; basename `pwd`)

include ${T_OBJ}/${CODENAME}-config.mk

unexport ARCH

ifeq (${ARCH},x86_64)
ARCH := x86
T_CC_FLAGS := -D __ELF_X86_64__
endif

include ${ARCH}/comp.mk

T_CC_BASE_FLAGS  := -I${ARCH}
T_CC_OPT_FLAGS   := -Os
T_CC_DEBUG_FLAGS := -Wall -ggdb

SRCFILES := $(shell find ${ARCH} '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g')

include ${T_BASE}/utl/template.mk

all: ${T_OBJ}/${CODENAME}-bootimage

${T_OBJ}/${CODENAME}-boot.bin: ${OBJFILES} ${ARCH}/bootloader.ld
	${PRINT} LD $@
	${V}${LD} -N -e start -T${ARCH}/bootloader.ld -o$@ ${OBJFILES}
	${V}${STRIP} -g -R .eh_frame $@

${T_OBJ}/${CODENAME}-bootsect: ${T_OBJ}/${CODENAME}-boot.bin
	${PRINT} OBJCOPY $@
	${V}${OBJCOPY} -S -O binary $^ $@.original
	${V}${T_OBJ}/${CODENAME}-tools-sign $@.original $@

${T_OBJ}/${CODENAME}-bootimage: ${T_OBJ}/${CODENAME}-bootsect ${T_OBJ}/${CODENAME}-kernel-bin
	${PRINT} MAKE $@
	${V}dd if=/dev/zero of=$@ bs=512 count=10000
	${V}dd if=${T_OBJ}/${CODENAME}-bootsect of=$@ bs=512 conv=notrunc
	${V}dd if=${T_OBJ}/${CODENAME}-kernel-bin of=$@ bs=512 seek=1 conv=notrunc
