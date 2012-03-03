INCLUDE +=		include
SRCDIR  :=		./

SRCFILES:= $(shell ${FIND} ${SRCDIR} -iname "*.c" -or -iname "*.S" | sed -e 's!\./!!g')

