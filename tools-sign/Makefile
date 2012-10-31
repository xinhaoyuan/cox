.PHONY: all

CODENAME := $(shell cd ..; basename `pwd`)
include ${T_OBJ}/${CODENAME}-config.mk

T_CC_OPT_FLAGS		:= -O2 
T_CC_DEBUG_FLAGS	:= -Wall
SRCFILES			:= $(shell find . '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g')

include ${T_BASE}/utl/template.mk

all: ${T_OBJ}/${PRJ}

${T_OBJ}/${PRJ}: ${OBJFILES}
	${PRINT} LD $@
	${V}${CC} -o$@ $^
