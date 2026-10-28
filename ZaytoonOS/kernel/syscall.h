#ifndef ZAYTOONOS_SYSCALL_H
#define ZAYTOONOS_SYSCALL_H

#include "kernel/types.h"

enum {
    SYS_EXIT = 1,
    SYS_READ = 3,
    SYS_WRITE = 4,
    SYS_BRK = 45,
};

struct syscall_frame {
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
    u32 eip;
    u32 cs;
    u32 eflags;
    u32 user_esp;
    u32 ss;
};

void syscall_dispatch(struct syscall_frame *frame);

#endif /* ZAYTOONOS_SYSCALL_H */
