include ${T_OBJ}/${CODENAME}-config.mk

PATH_VAR_ARCH := PATH_${CODENAME}-arch

ifeq (${ARCH},x86_64)

T_CC_ARCH_INC :=	-I ${${PATH_VAR_ARCH}}/noarch -I ${${PATH_VAR_ARCH}}/${ARCH}
ARCH_SRCFILES :=	${${PATH_VAR_ARCH}}/noarch ${${PATH_VAR_ARCH}}/${ARCH}

ifneq (,$(findstring NOSTD,${ARCH_FLAGS}))
T_CC_ARCH_INC +=	-I ${${PATH_VAR_ARCH}}/noarch/std -I ${${PATH_VAR_ARCH}}/${ARCH}/std
endif

endif
