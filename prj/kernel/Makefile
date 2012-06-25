.PHONY: all

include ${T_BASE}/config.mk
include ${T_OBJ}/arch.mk
include ${T_BASE}/prj/cox/${ARCH}/kern/comp.mk
include ${T_BASE}/prj/cox/${ARCH}/kern/user_init.mk

T_CC_FLAGS       := -D__KERN__ -DARCH=${ARCH} ${T_CC_ARCH_INC} -I${T_BASE}/prj/cox -I${T_BASE}/prj/cox/${ARCH} -Iinclude -Iinclude/${ARCH}
T_CC_OPT_FLAGS   := -O2
T_CC_DEBUG_FLAGS := -Wall -ggdb

SRCFILES := $(shell find ${ARCH} '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find noarch '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find ../arch/noarch '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \

include ${T_BASE}/utl/template.mk

OBJFILES += ${T_OBJ}/user_init.o

all: ${T_OBJ}/${PRJ}-image

${T_OBJ}/${PRJ}-bin: ${OBJFILES} ${T_BASE}/prj/cox/${ARCH}/kern/ldscript
	${PRINT} LD $@
	${V}${LD} -T ${T_BASE}/prj/cox/${ARCH}/kern/ldscript -z max-page-size=0x1000 -o$@ ${OBJFILES}

${T_OBJ}/${PRJ}-image: ${T_OBJ}/${PRJ}-bin
	${PRINT} OBJCOPY $@
	${V}${OBJCOPY} -S -Obinary $< $@
