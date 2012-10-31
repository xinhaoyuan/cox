.PHONY: all

PATH_VAR := PATH_${PRJ}
CODENAME := $(shell cd ..; basename `pwd`)

all: ${T_OBJ}/${CODENAME}-arch.mk

${T_OBJ}/${CODENAME}-config.mk: ${${PATH_VAR}}/../config.mk
	${V}ln -sf $< $@

-include ${T_OBJ}/${CODENAME}-config.mk

${T_OBJ}/${CODENAME}-arch.mk: ${${PATH_VAR}}/arch.mk
	${V}ln -sf $< $@
