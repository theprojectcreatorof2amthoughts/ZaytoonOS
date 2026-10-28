#include "kernel/syscall.h"
#include "kernel/kernel/printk.h"
#include "kernel/kernel/task.h"
#include "kernel/drivers/console.h"
#include "kernel/drivers/keyboard.h"

static void sys_write(struct syscall_frame *frame)
{
    const char *buf = (const char *)frame->ecx;
    u32 count = frame->edx;
    if ((frame->ebx != 1u && frame->ebx != 2u) || (!buf && count != 0)) {
        frame->eax = (u32)-1;
        return;
    }

    for (u32 i = 0; i < count; ++i) {
        console_write_char(buf[i]);
    }
    frame->eax = count;
}

static void sys_exit(struct syscall_frame *frame)
{
    printk("[syscall] process exited with status %d", (int)frame->ebx);
    task_exit_current();
}

static void sys_brk(struct syscall_frame *frame)
{
    frame->eax = 0;
}

static void sys_read(struct syscall_frame *frame)
{
    char *buf = (char *)frame->ecx;
    u32 count = frame->edx;
    if (frame->ebx != 0 || (!buf && count != 0)) {
        frame->eax = (u32)-1;
        return;
    }
    if (count == 0) {
        frame->eax = 0;
        return;
    }

    buf[0] = keyboard_getchar();
    u32 bytes_read = 1;
    while (bytes_read < count) {
        int c = keyboard_poll_char();
        if (c < 0) {
            break;
        }
        buf[bytes_read++] = (char)c;
    }
    frame->eax = bytes_read;
}

void syscall_dispatch(struct syscall_frame *frame)
{
    switch (frame->eax) {
    case SYS_WRITE:
        sys_write(frame);
        break;
    case SYS_EXIT:
        sys_exit(frame);
        break;
    case SYS_BRK:
        sys_brk(frame);
        break;
    case SYS_READ:
        sys_read(frame);
        break;
    default:
        printk("[syscall] unknown syscall %u", frame->eax);
        frame->eax = (u32)-1;
        break;
    }
}
