.PHONY: all

include ${T_BASE}/config.mk
include ${T_OBJ}/arch.mk
include ../cox/${ARCH}/kern/comp.mk
include ../cox/${ARCH}/kern/user_init.mk

T_CC_FLAGS       := -D__KERN__ -DARCH_TEST=1 -DARCH=${ARCH} ${T_CC_ARCH_INC} -I../cox -I../cox/${ARCH} -Iinclude -Iinclude/${ARCH}
T_CC_OPT_FLAGS   := -O2
T_CC_DEBUG_FLAGS := -Wall -ggdb

SRCFILES := $(shell find ${ARCH} '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find noarch '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find ../arch/noarch '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \

include ${T_BASE}/utl/template.mk

all: ${T_OBJ}/${PRJ}-image

${T_OBJ}/${PRJ}-bin: ${OBJFILES} ../cox/${ARCH}/kern/ldscript
	${PRINT} LD $@
	${V}${LD} -T ../cox/${ARCH}/kern/ldscript -z max-page-size=0x1000 -o$@ ${OBJFILES}

${T_OBJ}/${PRJ}-image: ${T_OBJ}/${PRJ}-bin
	${PRINT} OBJCOPY $@
	${V}${OBJCOPY} -S -Obinary $< $@
