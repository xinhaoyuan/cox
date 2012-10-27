.PHONY: cox-run cox-run-gdb cox-run-3w

cox-run: cox-bootimage
	${V}./cox/run console

cox-run-gdb: cox-bootimage
	${V}./cox/run gdb

cox-run-3w: cox-bootimage
	${V}./cox/run 3w
