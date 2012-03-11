.text
		.quad start
		.quad __entry
		.quad edata
		.quad end

.globl __entry

__entry:
		movq $0, %rbp
		call entry
