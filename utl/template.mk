E_ENCODE ?= $(shell echo $(1) | sed -e 's!_!_1!g' -e 's!/!_2!g')
E_DECODE ?= $(shell echo $(1) | sed -e 's!_2!/!g' -e 's!_1!_!g')

OBJFILES := $(addprefix ${T_OBJ}/${PRJ}-,$(addsuffix .o,$(foreach FILE,${SRCFILES},$(call E_ENCODE,${FILE}))))
DEPFILES := $(OBJFILES:.o=.d)

PRINT := @${T_BASE}/utl/myecho ${MAKELEVEL}

-include ${DEPFILES}

T_CC_ALL_FLAGS ?= ${T_CC_BASE_FLAGS} ${T_CC_OPT_FLAGS} ${T_CC_DEBUG_FLAGS} ${T_CC_FLAGS}

# ASM

${T_OBJ}/${PRJ}-%.S.d: 
	${V}${CC} -D__ASSEMBLY__ -MM ${T_CC_ALL_FLAGS} -MT $(@:.d=.o) $(call E_DECODE,$*).S -o$@
	${V}echo "$(@:.d=.o): $(call E_DECODE,$*).S" >> $@

${T_OBJ}/${PRJ}-%.S.o: ${T_OBJ}/${PRJ}-%.S.d
	${PRINT} CC $(call E_DECODE,$*).S
	${V}${CC} -D__ASSEMBLY__ ${T_CC_ALL_FLAGS} -c $(call E_DECODE,$*).S -o $@

# C

${T_OBJ}/${PRJ}-%.c.d: 
	${V}${CC} -MM ${T_CC_ALL_FLAGS} -MT $(@:.d=.o) $(call E_DECODE,$*).c -o$@
	${V}echo "$(@:.d=.o): $(call E_DECODE,$*).c" >> $@

${T_OBJ}/${PRJ}-%.c.o: ${T_OBJ}/${PRJ}-%.c.d
	${PRINT} CC $(call E_DECODE,$*).c
	${V}${CC} ${T_CC_ALL_FLAGS} -c $(call E_DECODE,$*).c -o $@
