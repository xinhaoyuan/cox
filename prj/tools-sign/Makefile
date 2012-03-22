.PHONY: all

include ${T_BASE}/config.mk
include ${T_OBJ}/arch.mk

T_CC_FLAGS    := -DARCH=${ARCH}
T_CC_OPT_FLAGS   := -O2 
T_CC_DEBUG_FLAGS := -Wall
SRCFILES      := $(shell find . '(' '!' -regex '.*/_.*' ')' -and '(' -iname "*.c" -or -iname "*.S" ')' | sed -e 's!^\./!!g')

include ${T_BASE}/utl/template.mk

all: ${T_OBJ}/tools-sign

${T_OBJ}/tools-sign: ${OBJFILES}
	${PRINT} LD $@
	${V}${CC} -o$@ $^
