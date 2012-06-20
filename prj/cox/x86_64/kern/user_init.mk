${T_OBJ}/user_init.o: ${T_OBJ}/user_init-image
	${V}(cd ${T_OBJ}; ${OBJCOPY} -I binary -O elf64-x86-64 -B "i386:x86-64" user_init-image user_init.o)
