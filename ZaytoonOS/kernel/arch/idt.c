#include "kernel/arch/idt.h"
#include "kernel/irq/irq.h"
#include "kernel/irq/pic.h"
#include "kernel/types.h"

struct idt_entry {
    u16 offset_low;
    u16 selector;
    u8  reserved;
    u8  type_attr;
    u16 offset_high;
} __attribute__((packed));

struct idt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idt_descriptor;

extern void default_interrupt_stub(void);
extern void timer_interrupt_stub(void);
extern void keyboard_interrupt_stub(void);
extern void syscall_entry(void);

static void set_idt_gate(u8 vector, void *handler, u16 selector, u8 type_attr)
{
    u32 addr = (u32)handler;
    idt[vector].offset_low  = (u16)(addr & 0xFFFF);
    idt[vector].selector    = selector;
    idt[vector].reserved    = 0;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_high = (u16)((addr >> 16) & 0xFFFF);
}

void default_interrupt_handler(void)
{
}

void timer_interrupt_handler(void)
{
    pic_eoi(0);
    irq_dispatch(32);
}

void keyboard_interrupt_handler(void)
{
    irq_dispatch(33);
    pic_eoi(1);
}

void idt_init(void)
{
    for (u16 vector = 0; vector < 256; ++vector) {
        set_idt_gate((u8)vector, default_interrupt_stub, 0x08, 0x8E);
    }

    set_idt_gate(32, timer_interrupt_stub, 0x08, 0x8E);
    set_idt_gate(33, keyboard_interrupt_stub, 0x08, 0x8E);
    set_idt_gate(0x80, syscall_entry, 0x08, 0xEE);

    idt_descriptor.limit = sizeof(idt) - 1;
    idt_descriptor.base  = (u32)&idt;
    asm volatile("lidt %0" : : "m"(idt_descriptor));
}
