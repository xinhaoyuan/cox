.text
		.quad __entry		
		.quad start
		.quad etext
		.quad edata
		.quad end

.globl __entry

__entry:
		movq $0, %rbp
		call entry
