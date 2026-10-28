#ifndef TASK_H
#define TASK_H

#include <stdbool.h>
#include <stdint.h> // Use standard sizes here too!
#include "kernel/kernel/context.h"

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_STOPPED
} task_state_t;

typedef struct task {
    const char *name;
    bool user_task;
    task_state_t state;
    void (*entry)(void);
    void *stack;
    uint32_t user_entry;      
    uint32_t user_stack;      
    uint32_t *page_table;     
    struct task *next;
    context_t context;
} task_t;

void task_init(void);
void task_add(void (*entry)(void), const char *name);
void task_add_user(uint32_t entry, uint32_t stack, uint32_t *page_table, const char *name);
void task_yield(void);
void task_exit_current(void);
void task_run(void);

#endif // TASK_H