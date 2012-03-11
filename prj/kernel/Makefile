.PHONY: all

include ${T_BASE}/config.mk
include ${T_OBJ}/arch.mk
include ${ARCH}/comp.mk
include ${ARCH}/user.mk

T_CC_FLAGS       := -D__KERN__ -DARCH=${ARCH} ${T_CC_ARCH_INC} -Iinclude -Iinclude/kernel -Iinclude/${ARCH} -Iinclude/${ARCH}/kernel -Ilwip/src/include -Ilwip/src/include/ipv4
T_CC_OPT_FLAGS   := -O2
T_CC_DEBUG_FLAGS := -Wall -ggdb

SRCFILES := $(shell find ${ARCH} '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!\./!!g') \
			$(shell find model '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!\./!!g') \
			$(shell find lwip/src '(' '!' -regex '.*/_.*' ')' -and '(' '!' -regex '.*ipv6.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' \
				| sed -e 's!\./!!g')

include ${T_BASE}/utl/template.mk

all: ${T_OBJ}/${PRJ}-image

${T_OBJ}/${PRJ}-bin: ${OBJFILES} ${T_OBJ}/user_init.o ${ARCH}/ldscript
	${PRINT} LD $@
	${V}${LD} -T ${ARCH}/ldscript -z max-page-size=0x1000 -o$@ ${OBJFILES} ${T_OBJ}/user_init.o

${T_OBJ}/${PRJ}-image: ${T_OBJ}/${PRJ}-bin
	${PRINT} OBJCOPY $@
	${V}${OBJCOPY} -S -Obinary $< $@
