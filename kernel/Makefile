.PHONY: all

include ${T_OBJ}/arch.mk
include ${PATH_cox-inc}/${ARCH}/kern/comp.mk
include ${PATH_cox-inc}/${ARCH}/kern/user_init.mk

T_CC_FLAGS       := -D __KERN__ -D ARCH=${ARCH} ${T_CC_ARCH_INC} -I ${PATH_cox-inc} -I ${PATH_cox-inc}/${ARCH} -I include -I include/${ARCH}
T_CC_OPT_FLAGS   := -O2
T_CC_DEBUG_FLAGS := -Wall -ggdb

SRCFILES := $(shell find ${ARCH} '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find noarch '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find ${PATH_cox-arch}/noarch '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \

include ${T_BASE}/utl/template.mk

OBJFILES += ${T_OBJ}/cox-user_init.o

all: ${T_OBJ}/${PRJ}-image

${T_OBJ}/${PRJ}-bin: ${OBJFILES} ${PATH_cox-inc}/${ARCH}/kern/ldscript
	${PRINT} LD $@
	${V}${LD} -T ${PATH_cox-inc}/${ARCH}/kern/ldscript -z max-page-size=0x1000 -o$@ ${OBJFILES}

${T_OBJ}/${PRJ}-image: ${T_OBJ}/${PRJ}-bin
	${PRINT} OBJCOPY $@
	${V}${OBJCOPY} -S -Obinary $< $@
