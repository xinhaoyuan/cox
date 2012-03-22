include ${T_BASE}/config.mk

ifeq (${ARCH},x86_64)

T_CC_ARCH_INC :=	-I${T_BASE}/prj/arch/noarch/std \
					-I${T_BASE}/prj/arch/${ARCH}/asm \
					-I${T_BASE}/prj/arch/${ARCH}/std

endif
