.PHONY: all

CODENAME := $(shell cd ..; basename `pwd`)
include ${T_OBJ}/${CODENAME}-arch.mk

PATH_VAR_INCLUDE := PATH_${CODENAME}-include
PATH_VAR_ARCH    := PATH_${CODENAME}-arch
include ${${PATH_VAR_INCLUDE}}/${ARCH}/kern/comp.mk
include ${${PATH_VAR_INCLUDE}}/${ARCH}/kern/user_init.mk

T_CC_FLAGS       := -D __KERN__ -D ARCH=${ARCH} ${T_CC_ARCH_INC} -I ${${PATH_VAR_INCLUDE}} -I ${${PATH_VAR_INCLUDE}}/${ARCH} -I include -I include/${ARCH}
T_CC_OPT_FLAGS   := -O2
T_CC_DEBUG_FLAGS := -Wall -ggdb

SRCFILES := $(shell find ${ARCH} '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find noarch '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \
			$(shell find ${${PATH_VAR_ARCH}}/noarch '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g') \

include ${T_BASE}/utl/template.mk

OBJFILES += ${T_OBJ}/${CODENAME}-user_init.o

all: ${T_OBJ}/${PRJ}-image

${T_OBJ}/${PRJ}-bin: ${OBJFILES} ${${PATH_VAR_INCLUDE}}/${ARCH}/kern/ldscript
	${PRINT} LD $@
	${V}${LD} -T ${${PATH_VAR_INCLUDE}}/${ARCH}/kern/ldscript -z max-page-size=0x1000 -o$@ ${OBJFILES}

${T_OBJ}/${PRJ}-image: ${T_OBJ}/${PRJ}-bin
	${PRINT} OBJCOPY $@
	${V}${OBJCOPY} -S -Obinary $< $@
