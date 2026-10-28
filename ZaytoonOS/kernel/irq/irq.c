#include "kernel/irq/irq.h"
#include "kernel/irq/pic.h"

static irq_handler_t irq_table[256];

void irq_init(void)
{
    for (unsigned int i = 0; i < 256; ++i) {
        irq_table[i] = 0;
    }

    pic_remap();
    pic_unmask(0);  /* Timer IRQ */
}

void irq_disable(void)
{
    asm volatile("cli");
}

void register_irq_handler(unsigned int irq, irq_handler_t handler)
{
    if (irq < 256) {
        irq_table[irq] = handler;
    }
}

void irq_dispatch(unsigned int irq)
{
    if (irq < 256 && irq_table[irq]) {
        irq_table[irq]();
    }
}

void irq_enable(void)
{
    asm volatile("sti");
}
