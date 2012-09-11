${T_OBJ}/cox-user_init.o: ${T_OBJ}/cox-user_init-image
	${V}(cd ${T_OBJ}; ${OBJCOPY} -I binary -O elf64-x86-64 -B "i386:x86-64" cox-user_init-image cox-user_init.o)
