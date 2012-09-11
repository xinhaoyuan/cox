.text
        .quad __bin_start
        .quad __init_entry
        .quad __bin_edata
        .quad __bin_end

.globl __init_entry

__init_entry:
        movq $0, %rbp
        movq %rsp, %rax
        testq %rax, %rax
        jnz goinit
        movq $init_stack, %rsp
goinit: 
        call __init

.bss
        .space 4096
init_stack:     
