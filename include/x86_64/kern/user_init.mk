${T_OBJ}/${CODENAME}-user_init.o: ${T_OBJ}/${CODENAME}-user_init-image
	${V}(cd ${T_OBJ} && ${OBJCOPY} -I binary -O elf64-x86-64 -B "i386:x86-64" ${CODENAME}-user_init-image ${CODENAME}-user_init.o \
			--redefine-sym _binary_${CODENAME}_user_init_image_start=_user_init_image_start \
			--redefine-sym _binary_${CODENAME}_user_init_image_end=_user_init_image_end     \
			--redefine-sym _binary_${CODENAME}_user_init_image_size=_user_init_image_size)
