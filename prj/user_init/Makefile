.PHONY: all

include ${T_BASE}/config.mk
include ${T_OBJ}/arch.mk
include ${T_BASE}/prj/kernel/include/${ARCH}/user/comp.mk

T_CC_FLAGS       := -DARCH=${ARCH} ${T_CC_ARCH_INC} -I${T_BASE}/prj/kernel/include -I${T_BASE}/prj/kernel/include/${ARCH} -Iinclude -I${ARCH}
T_CC_OPT_FLAGS   := -O0
T_CC_DEBUG_FLAGS := -Wall -ggdb

SRCFILES := $(shell find ${T_BASE}/prj/kernel/include/${ARCH}/user \
				'(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!\./!!g') \
			$(shell find model '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!\./!!g') \
			$(shell find lib '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!\./!!g') \
			$(shell find runtime '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!\./!!g') \
			$(shell find ${ARCH} '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!\./!!g')

include ${T_BASE}/utl/template.mk

all: ${T_OBJ}/${PRJ}-image

${T_OBJ}/${PRJ}-bin: ${OBJFILES} ${T_BASE}/prj/kernel/include/${ARCH}/user/ldscript
	${PRINT} LD $@
	${V}${LD} -T ${T_BASE}/prj/kernel/include/${ARCH}/user/ldscript -z max-page-size=0x1000 -o$@ ${OBJFILES}

${T_OBJ}/${PRJ}-image: ${T_OBJ}/${PRJ}-bin
	${PRINT} OBJCOPY $@
	${V}${OBJCOPY} -S -Obinary $< $@
