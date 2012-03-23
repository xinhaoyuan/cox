.text
		.quad start
		.quad __init_entry
		.quad edata
		.quad end

.globl __init_entry

__init_entry:
		movq $0, %rbp
		movq $init_stack, %rsp
		call __init

.bss
		.space 4096
init_stack:		
