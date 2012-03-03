.PHONY: all clean

include ${P_BASE}/global/utils/make/target
include ${P_BASE}/global/utils/make/local
include ${P_BASE}/global/utils/make/cmds
include ${P_BASE}/global/utils/make/escape

all: ${T_BASE}/mod_${MOD_NAME}-X86EMU_TIMESTAMP

INCLUDE :=		./ \
				include/ ${P_BASE}

T_CC_FLAGS := 	${T_CC_GLOBAL_FLAGS} -gstabs -D__X86EMU__ $(addprefix ${T_CC_FLAG_INC},$(INCLUDE)) -nostdlib -nostdinc -fno-builtin

SRCFILES:= $(shell $(call C_FIND,.,-iname '*.c' -or -iname '*.S') | $(call C_SED,-e 's!\./!!g'))
OBJFILES:= $(addprefix ${T_BASE}/mod_${MOD_NAME}-X86EMU_,\
		     $(addsuffix .o,$(call E_ENCODE,${SRCFILES})))
DEPFILES:= $(OBJFILES:.o=.d)

-include $(DEPFILES)

${T_BASE}/mod_${MOD_NAME}-X86EMU_%.S.d: 
	@$(call C_ECHO,DEP $(call E_DECODE,$*).S)
	${V}$(call T_CC, $(call E_DECODE,$*).S,$@,-D__ASSEMBLY__ -MM ${T_CC_FLAGS} -MT $(@:.d=.o))
	${V}$(call C_ECHO,"$(@:.d=.o): $(call E_DECODE,$*).S") >> $@

${T_BASE}/mod_${MOD_NAME}-X86EMU_%.c.d: 
	${V}$(call C_ECHO,DEP $(call E_DECODE,$*).c)
	${V}$(call T_CC, $(call E_DECODE,$*).c,$@,-MM ${T_CC_FLAGS} -MT $(@:.d=.o))
	${V}$(call C_ECHO,"$(@:.d=.o): $(call E_DECODE,$*).c") >> $@

${T_BASE}/mod_${MOD_NAME}-X86EMU_%.S.o: ${T_BASE}/mod_${MOD_NAME}-X86EMU_%.S.d
	@$(call C_ECHO,CC $(call E_DECODE,$*).S)
	${V}$(call T_CC,$(call E_DECODE,$*).S,$@,-D__ASSEMBLY__ ${T_CC_FLAGS} ${T_CC_FLAG_OBJ})

${T_BASE}/mod_${MOD_NAME}-X86EMU_%.c.o: ${T_BASE}/mod_${MOD_NAME}-X86EMU_%.c.d
	@$(call C_ECHO,CC $(call E_DECODE,$*).c)
	${V}$(call T_CC,$(call E_DECODE,$*).c,$@,${T_CC_FLAGS} ${T_CC_FLAG_OBJ})

${T_BASE}/mod_${MOD_NAME}-X86EMU_TIMESTAMP: ${OBJFILES}
	@$(call C_ECHO,TOUCH $@)
	${V}touch $@
