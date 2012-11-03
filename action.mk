.PHONY: ${PRJ}-run ${PRJ}-run-gdb ${PRJ}-run-3w

PATH_VAR := PATH_${PRJ}

${PRJ}-run: ${PRJ}
	${V}${${PATH_VAR}}/run.sh $< ${T_OBJ} console

${PRJ}-run-gdb: ${PRJ}
	${V}${${PATH_VAR}}/run.sh $< ${T_OBJ} gdb

${PRJ}-run-3w: ${PRJ}
	${V}${${PATH_VAR}}/run.sh $< ${T_OBJ} 3w

${PRJ}-git-commit: ${PRJ}
	${V}cd ${${PATH_VAR}}; git add . ; git commit -a -m "commit through action.mk"; git push
