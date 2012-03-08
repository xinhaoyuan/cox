.PHONY: all

include ${T_BASE}/config.mk
include ${T_OBJ}/arch.mk
include ${ARCH}/comp.mk

T_CC_FLAGS       := -DARCH=${ARCH} ${T_CC_ARCH_INC} -I${T_BASE}/prj/kernel/include -I${T_BASE}/prj/kernel/include/${ARCH} -Imodel -I${ARCH}
T_CC_OPT_FLAGS   := -O0
T_CC_DEBUG_FLAGS := -Wall -ggdb

SRCFILES := $(shell find ${ARCH} '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!\./!!g') \
			$(shell find model '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!\./!!g')

include ${T_BASE}/utl/template.mk

all: ${T_OBJ}/${PRJ}-image

${T_OBJ}/${PRJ}-bin: ${OBJFILES} ${ARCH}/ldscript
	${PRINT} LD $@
	${V}${LD} -T ${ARCH}/ldscript -z max-page-size=0x1000 -o$@ ${OBJFILES}

${T_OBJ}/${PRJ}-image: ${T_OBJ}/${PRJ}-bin
	${PRINT} OBJCOPY $@
	${V}${OBJCOPY} -S -Obinary $< $@
