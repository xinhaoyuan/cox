.PHONY: all

CODENAME := $(shell cd ..; basename `pwd`)
include ${T_OBJ}/${CODENAME}-arch.mk

PATH_VAR_INCLUDE := PATH_${CODENAME}-include
PATH_VAR_ARCH    := PATH_${CODENAME}-arch
include ${${PATH_VAR_INCLUDE}}/${ARCH}/user/comp.mk

T_CC_FLAGS       := -D ARCH=${ARCH} ${T_CC_ARCH_INC} -I ${${PATH_VAR_INCLUDE}}/ -I ${${PATH_VAR_INCLUDE}}/${ARCH} -I ${${PATH_VAR_INCLUDE}}/${ARCH}/user -I include
T_CC_OPT_FLAGS   := -O0
T_CC_DEBUG_FLAGS := -Wall -ggdb

SRCFILES := $(shell find ${${PATH_VAR_INCLUDE}}/${ARCH}/user \
				'(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find noarch '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find ${${PATH_VAR_ARCH}}/noarch '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find ${ARCH} '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g')

include ${T_BASE}/utl/template.mk

all: ${T_OBJ}/${PRJ}-image

${T_OBJ}/${PRJ}-bin: ${OBJFILES} ${${PATH_VAR_INCLUDE}}/${ARCH}/user/ldscript
	${PRINT} LD $@
	${V}${LD} -T ${${PATH_VAR_INCLUDE}}/${ARCH}/user/ldscript -z max-page-size=0x1000 -o$@ ${OBJFILES}

${T_OBJ}/${PRJ}-image: ${T_OBJ}/${PRJ}-bin
	${PRINT} OBJCOPY $@
	${V}${OBJCOPY} -S -Obinary $< $@
