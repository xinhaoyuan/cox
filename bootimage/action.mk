.PHONY: cox-run cox-run-gdb cox-run-3w

cox-run: cox-bootimage
	${V}${PATH_cox-bootimage}/../run ${T_OBJ} console

cox-run-gdb: cox-bootimage
	${V}${PATH_cox-bootimage}/../run ${T_OBJ} gdb

cox-run-3w: cox-bootimage
	${V}${PATH_cox-bootimage}/../run ${T_OBJ} 3w
