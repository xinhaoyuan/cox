.PHONY: all

include ${T_OBJ}/cox-arch.mk
include ${PATH_cox-include}/${ARCH}/user/comp.mk

T_CC_FLAGS       := -D ARCH=${ARCH} ${T_CC_ARCH_INC} -I ${PATH_cox-include}/ -I ${PATH_cox-include}/${ARCH} -I ${PATH_cox-include}/${ARCH}/user -I include
T_CC_OPT_FLAGS   := -O0
T_CC_DEBUG_FLAGS := -Wall -ggdb

SRCFILES := $(shell find ${PATH_cox-include}/${ARCH}/user \
				'(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find noarch '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find ${PATH_cox-arch}/noarch '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find ${ARCH} '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g')

include ${T_BASE}/utl/template.mk

all: ${T_OBJ}/${PRJ}-image

${T_OBJ}/${PRJ}-bin: ${OBJFILES} ${PATH_cox-include}/${ARCH}/user/ldscript
	${PRINT} LD $@
	${V}${LD} -T ${PATH_cox-include}/${ARCH}/user/ldscript -z max-page-size=0x1000 -o$@ ${OBJFILES}

${T_OBJ}/${PRJ}-image: ${T_OBJ}/${PRJ}-bin
	${PRINT} OBJCOPY $@
	${V}${OBJCOPY} -S -Obinary $< $@
