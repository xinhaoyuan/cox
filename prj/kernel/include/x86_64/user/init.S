.text
		.quad start
		.quad __entry
		.quad edata
		.quad end

.globl __entry

__entry:
		movq $0, %rbp
		movq $init_stack, %rsp
		call entry

.bss
		.space 4096
init_stack:		
