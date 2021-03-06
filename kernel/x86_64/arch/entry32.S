#include <asm/mmu.h>
#include <arch/memlayout.h>
#include "seg_boot.h"

#define MULTIBOOT_PAGE_ALIGN   (1<<0)
#define MULTIBOOT_MEMORY_INFO  (1<<1)
#define MULTIBOOT_AOUT_KLUDGE  (1<<16)
#define MULTIBOOT_HEADER_MAGIC (0x1BADB002)
#define MULTIBOOT_HEADER_FLAGS (MULTIBOOT_MEMORY_INFO | MULTIBOOT_PAGE_ALIGN | MULTIBOOT_AOUT_KLUDGE)
#define CHECKSUM               (-(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS))

              .text
              .code32
kern_start32:

# Multi Boot Header
              .align 4
              .long MULTIBOOT_HEADER_MAGIC
              .long MULTIBOOT_HEADER_FLAGS
              .long CHECKSUM

              .long kern_start32
              .long __text
              .long __bss
              .long __end
              .long kern_entry32

              .global kern_entry32
kern_entry32:
              movl  %eax, mb_magic - KERNBASE
              movl  %ebx, mb_info_phys - KERNBASE

# Detection of CPUID:
# Basically, detecting wheterh CPUID is supported is covered here.
# CPUID is supported when the ID-bit in the FLAGS-register can be flipped.
              pushf # Store FLAGS-register to stack
              popl  %eax                                       # Restore the A-register
              movl  %eax, %ecx                                 # Copy the A-register to C-register
              xorl  $FL_ID, %eax                               # Flip the ID-bit
              pushl %eax                                      # Store the A-register
              popf  # Restore the FLAGS-register

              pushf # Store the FLAGS-register
              popl  %eax                                       # Restore the A-register
              pushl %ecx                                      # Store the C-register
              popf  # Restore the FLAGS-register
              xor   %eax, %ecx                                  # Do a XOR-operation
              jz    spin_nocpuid

# Detect x86 or x86_64
              movl  $0x80000000, %eax                          # Set the A-register to $0x80000000
              cpuid # CPU identification
              cmpl  $0x80000001, %eax                          # Compare the A-register with $0x80000001
              jb    spin_nolongm                                 # It is less, there is no long mode.

              movl  $0x80000001, %eax                          # Set the A-register to $0x80000001
              cpuid # CPU identification
              test  $0x20000000, %edx                          # Test if the LM-bit is set in the D-register
              jz    spin_nolongm                                 # They aren't, there is no long mode.

# Build page table for long-mode
              cld

# step1: set pgd entry, mark pud as (PTE_W | PTE_P)
              movl  $pgd, %edi
              movl  $pud, %eax
              orl   $0x3, %eax
              movl  %eax, (%edi)
              movl  %eax, 0xFF8(%edi)      # For KERNBASE
              movl  %eax, 0x900(%edi)      # For PHYSBASE

# clear and set pud, mark pmd as (PTE_W | PTE_P)
              movl  $pud, %edi
              movl  $pmd, %eax
              orl   $0x3, %eax
              movl  %eax, 0xFF0(%edi)      # FOR KERNBASE
              movl  %eax, (%edi)

# set pmd, Mark each entry as (PTE_W | PTE_P | PTE_PS)
              movl  $pmd, %edi
              movl  $0x83, %ebx
              movl  $0x200, %ecx                               # 512 entries

# map 2M * 512 = 1G memory
loop:
              movl  %ebx, (%edi)
              addl  $0x8, %edi
              addl  $0x200000, %ebx
              subl  $0x1, %ecx
              cmpl  $0x0, %ecx
              jne   loop

# Prepare for long-mode, set (CR4_PGE | CR4_PAE)
              movl  $0x20, %eax
              movl  %eax, %cr4

# set cr3
              movl  $pgd, %eax
              movl  %eax, %cr3

# enable long-mode
              movl  $0xC0000080, %ecx
              rdmsr
              orl   $0x00000100, %eax
              wrmsr

# Active long-mode
              movl  %cr0, %eax
              orl   $0x80000001, %eax
              movl  %eax, %cr0

              movl  $gdtdesc, %edi
              lgdt  (%edi)
              ljmp  $KERNEL_CS, $longmode_entry

spin:
              jmp   spin

spin_nocpuid:
              jmp   spin_nocpuid

spin_nolongm:
              jmp   spin_nolongm

# long-mode code here
              .code64
longmode_entry:
# Set up the protected-mode data segment registers
              mov   $KERNEL_DS, %ax
              mov   %ax, %ds
              mov   %ax, %es
              mov   %ax, %fs
              mov   %ax, %ss

# Jump to kern_entry64
              movq  $KERNBASE, %rax
              addq  $kern_entry64 - KERNBASE, %rax
              jmp   *%rax

              .data
              .align 4
gdt:
              SEG_NULL()
              SEG_CODE(STA_X | STA_R)
              SEG_DATA(STA_W)

gdtdesc:
              .word 0x2f
              .quad gdt

              .align PGSIZE
pgd:
              .rept 1024
              .long 0x0
              .endr
pud:
              .rept 1024
              .long 0x0
              .endr
pmd:
              .rept 1024
              .long 0x0
              .endr

