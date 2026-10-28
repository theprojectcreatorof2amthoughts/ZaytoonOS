.intel_syntax noprefix

.section .note.GNU-stack,"",@progbits
.section .text

.global syscall_entry
.type syscall_entry, @function
syscall_entry:
    pushad
    push esp
    call syscall_dispatch
    add esp, 4
    popad
    iret