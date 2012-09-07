include ${T_OBJ}/config.mk

ifeq (${ARCH},x86_64)

T_CC_ARCH_INC :=	-I ${PATH_cox-arch}/noarch -I ${PATH_cox-arch}/${ARCH}
ARCH_SRCFILES :=	${PATH_cox-arch}/noarch ${PATH_cox-arch}/${ARCH}

ifneq (,$(findstring NOSTD,${ARCH_FLAGS}))
T_CC_ARCH_INC +=	-I ${PATH_cox-arch}/noarch/std -I ${PATH_cox-arch}/${ARCH}/std
endif

endif
