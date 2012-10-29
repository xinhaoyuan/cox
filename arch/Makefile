.PHONY: all

${T_OBJ}/cox-config.mk: ${PATH_cox-arch}/../config.mk
	${V}ln -sf $< $@

-include ${T_OBJ}/cox-config.mk

all: ${T_OBJ}/cox-arch.mk

${T_OBJ}/cox-arch.mk: ${PATH_cox-arch}/arch.mk
	${V}ln -sf $< $@
