.PHONY: all

${T_OBJ}/cox-config.mk: ${PATH_cox-arch}/../config.mk
	${V}ln -sf $< $@

include ${T_OBJ}/cox-config.mk

all: ${T_OBJ}/arch.mk

${T_OBJ}/arch.mk:
	${V}ln -sf ${PATH_cox-arch}/arch.mk $@
