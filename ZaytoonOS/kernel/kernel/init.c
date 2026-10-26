#include "kernel/kernel/init.h"
#include "kernel/kernel/printk.h"
#include "kernel/drivers/console.h"
#include "kernel/mm/mm.h"
#include "kernel/kernel/sched.h"
#include "kernel/kernel/task.h"
#include "kernel/irq/irq.h"
#include "kernel/arch/arch.h"
#include "kernel/apps/capp.h"
#include "kernel/drivers/keyboard.h"
#include "kernel/fs/vfs.h"

static void shell_app_task(void)
{
    const char *argv[] = { "shell.capp" };
    if (capp_exec("shell.capp", 1, argv) != 0) {
        printk("[kernel] failed to launch shell.capp");
    }
}

void kernel_init(void)
{
    console_init();
    mm_init();
    task_init();
    capp_init();
    vfs_init();
    keyboard_init();
    task_add(shell_app_task, "shell.capp");
    task_add(kernel_idle, "idle");
    sched_init();
    register_irq_handler(32, schedule);
    printk("[kernel] ZaytoonOS initialized - waiting for keyboard input");
    irq_enable();
}

void kernel_idle(void)
{
    while (1) {
        arch_halt();
    }
}
