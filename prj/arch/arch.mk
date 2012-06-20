include ${T_BASE}/config.mk

ifeq (${ARCH},x86_64)

T_CC_ARCH_INC :=	-I${T_BASE}/prj/arch/noarch -I${T_BASE}/prj/arch/${ARCH}
ARCH_SRCFILES :=	prj/arch/noarch ${T_BASE}/prj/arch/${ARCH}

ifneq (,$(findstring NOSTD,${ARCH_FLAGS}))
T_CC_ARCH_INC +=	-I${T_BASE}/prj/arch/noarch/std -I${T_BASE}/prj/arch/${ARCH}/std
endif

endif
