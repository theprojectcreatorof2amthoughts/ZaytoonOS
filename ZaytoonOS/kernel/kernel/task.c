#include "kernel/arch/arch.h"
#include "kernel/arch/user_entry.h"
#include "kernel/kernel/context.h"
#include "kernel/kernel/task.h"
#include "kernel/kernel/printk.h"
#include "kernel/mm/mm.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KERNEL_TASK_STACK_SIZE 8192

static task_t *current_task;
static task_t *task_list;

static void task_wrapper(void)
{
    asm volatile("sti" : : : "memory");
    if (current_task && current_task->entry) {
        current_task->entry();
    }

    current_task->state = TASK_STOPPED;
    task_yield();

    for (;;) {
        arch_halt();
    }
}

void task_init(void)
{
    task_list = NULL;
    current_task = NULL;
}

void task_add(void (*entry)(void), const char *name)
{
    task_t *task = kmalloc(sizeof(task_t));
    void *stack = kmalloc(KERNEL_TASK_STACK_SIZE);
    if (!task || !stack) {
        printk("[task] allocation failed for %s", name);
        return;
    }

    uint32_t stack_top = (uint32_t)stack + KERNEL_TASK_STACK_SIZE;
    stack_top &= ~0xFul;
    stack_top -= sizeof(uint32_t);
    *(uint32_t *)stack_top = (uint32_t)task_wrapper;

    task->name = name;
    task->user_task = false;
    task->state = TASK_READY;
    task->entry = entry;
    task->stack = stack;
    task->user_entry = 0;
    task->user_stack = 0;
    task->page_table = NULL;
    task->next = NULL;

    task->context.edi = 0;
    task->context.esi = 0;
    task->context.ebx = 0;
    task->context.ebp = 0;
    task->context.esp = stack_top;

    if (!task_list) {
        task_list = task;
    } else {
        task_t *tail = task_list;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = task;
    }

    printk("[task] registered %s", name);
}

void task_add_user(uint32_t entry, uint32_t stack, uint32_t *page_table, const char *name)
{
    task_t *task = kmalloc(sizeof(task_t));
    if (!task) {
        printk("[task] allocation failed for %s", name);
        return;
    }

    task->name = name;
    task->user_task = true;
    task->state = TASK_READY;
    task->entry = NULL;
    task->stack = NULL;
    task->user_entry = entry;
    task->user_stack = stack;
    task->page_table = page_table;
    task->next = NULL;
    task->context.edi = 0;
    task->context.esi = 0;
    task->context.ebx = 0;
    task->context.ebp = 0;
    task->context.esp = 0;

    if (!task_list) {
        task_list = task;
    } else {
        task_t *tail = task_list;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = task;
    }

    printk("[task] registered user task %s", name);
}

void task_yield(void)
{
    if (!current_task) {
        return;
    }

    task_t *prev = current_task;
    task_t *next = prev;
    do {
        next = next->next ? next->next : task_list;
    } while (next != prev && next->state == TASK_STOPPED);

    if (next == prev || next->state == TASK_STOPPED) {
        return;
    }

    if (prev->state != TASK_STOPPED) {
        prev->state = TASK_READY;
    }
    current_task = next;
    current_task->state = TASK_RUNNING;
    context_switch(&prev->context, &current_task->context);
}

void task_exit_current(void)
{
    if (current_task) {
        current_task->state = TASK_STOPPED;
        task_yield();
    }

    for (;;) {
        asm volatile("sti; hlt" : : : "memory");
    }
}

void task_run(void)
{
    if (!task_list) {
        return;
    }

    current_task = task_list;
    current_task->state = TASK_RUNNING;

    if (current_task->user_task) {
        enter_user_mode(current_task->user_entry, current_task->user_stack);
    } else {
        /* Jump into the first kernel task by switching context from a dummy */
        context_t dummy;
        context_switch(&dummy, &current_task->context);
    }
}
