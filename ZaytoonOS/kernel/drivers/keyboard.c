#include "kernel/drivers/keyboard.h"
#include "kernel/arch/io.h"
#include "kernel/kernel/printk.h"
#include "kernel/irq/pic.h"
#include "kernel/types.h"

#define COM1_BASE       0x3F8u
#define COM1_DATA       (COM1_BASE + 0u)
#define COM1_STATUS     (COM1_BASE + 5u)

#define UART_LSR_DATA   0x01u
#define UART_LSR_THRE   0x20u
#define PS2_DATA        0x60u
#define PS2_STATUS      0x64u
#define KEYBOARD_BUFFER_SIZE 128u

static bool shift_pressed;
static bool caps_lock;
static bool extended_scancode;
static volatile char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile u32 keyboard_buffer_head;
static volatile u32 keyboard_buffer_tail;

static const char scancode_map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '/', [0x37] = '*', [0x39] = ' ',
};

static const char shifted_scancode_map[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
    [0x1A] = '{', [0x1B] = '}', [0x27] = ':', [0x28] = '"',
    [0x29] = '~', [0x2B] = '|', [0x33] = '<', [0x34] = '>',
    [0x35] = '?',
};

static int serial_data_ready(void)
{
    return (inb(COM1_STATUS) & UART_LSR_DATA) != 0;
}

static int serial_transmit_ready(void)
{
    return (inb(COM1_STATUS) & UART_LSR_THRE) != 0;
}

static void serial_put_char(char c)
{
    while (!serial_transmit_ready()) {
        asm volatile("pause");
    }
    outb(COM1_DATA, (uint8_t)c);
}

static bool ps2_wait_input_empty(void)
{
    for (u32 timeout = 0; timeout < 100000u; ++timeout) {
        if ((inb(PS2_STATUS) & 0x02u) == 0) {
            return true;
        }
        asm volatile("pause");
    }
    return false;
}

static bool ps2_wait_output_full(void)
{
    for (u32 timeout = 0; timeout < 100000u; ++timeout) {
        if ((inb(PS2_STATUS) & 0x01u) != 0) {
            return true;
        }
        asm volatile("pause");
    }
    return false;
}

static bool ps2_enable_keyboard(void)
{
    if (!ps2_wait_input_empty()) {
        return false;
    }
    outb(PS2_STATUS, 0x20u);
    if (!ps2_wait_output_full()) {
        return false;
    }
    u8 controller_config = inb(PS2_DATA);
    controller_config = (u8)((controller_config & (u8)~0x10u) | 0x01u);

    if (!ps2_wait_input_empty()) {
        return false;
    }
    outb(PS2_STATUS, 0x60u);
    if (!ps2_wait_input_empty()) {
        return false;
    }
    outb(PS2_DATA, controller_config);
    if (!ps2_wait_input_empty()) {
        return false;
    }
    outb(PS2_STATUS, 0xAEu);
    return ps2_wait_input_empty();
}

static u32 keyboard_irq_save(void)
{
    u32 flags;
    asm volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void keyboard_irq_restore(u32 flags)
{
    if (flags & 0x200u) {
        asm volatile("sti" : : : "memory");
    } else {
        asm volatile("cli" : : : "memory");
    }
}

static int keyboard_dequeue(void)
{
    if (keyboard_buffer_tail == keyboard_buffer_head) {
        return -1;
    }

    char c = keyboard_buffer[keyboard_buffer_tail];
    keyboard_buffer_tail = (keyboard_buffer_tail + 1u) % KEYBOARD_BUFFER_SIZE;
    return (unsigned char)c;
}

static void keyboard_enqueue(char c)
{
    u32 next_head = (keyboard_buffer_head + 1u) % KEYBOARD_BUFFER_SIZE;
    if (next_head != keyboard_buffer_tail) {
        keyboard_buffer[keyboard_buffer_head] = c;
        keyboard_buffer_head = next_head;
    }
}

static int ps2_translate_scancode(u8 scancode)
{
    if (scancode == 0xE0u || scancode == 0xE1u) {
        extended_scancode = true;
        return -1;
    }
    if (extended_scancode) {
        extended_scancode = false;
        return -1;
    }
    if (scancode == 0x2Au || scancode == 0x36u) {
        shift_pressed = true;
        return -1;
    }
    if (scancode == 0xAAu || scancode == 0xB6u) {
        shift_pressed = false;
        return -1;
    }
    if (scancode == 0x3Au) {
        caps_lock = !caps_lock;
        return -1;
    }
    if (scancode & 0x80u) {
        return -1;
    }

    char c = scancode_map[scancode];
    if (c >= 'a' && c <= 'z') {
        if (shift_pressed != caps_lock) {
            c = (char)(c - ('a' - 'A'));
        }
    } else if (shift_pressed && shifted_scancode_map[scancode]) {
        c = shifted_scancode_map[scancode];
    }
    return c ? (unsigned char)c : -1;
}

void keyboard_init(void)
{
    shift_pressed = false;
    caps_lock = false;
    extended_scancode = false;
    keyboard_buffer_head = 0;
    keyboard_buffer_tail = 0;

    for (u32 count = 0; count < 32u && (inb(PS2_STATUS) & 0x01u); ++count) {
        (void)inb(PS2_DATA);
    }

    if (!ps2_enable_keyboard()) {
        printk("[keyboard] PS/2 controller unavailable; serial input remains enabled");
        return;
    }
    pic_unmask(1);
}

void keyboard_irq_handler(void)
{
    u8 status = inb(PS2_STATUS);
    if ((status & 0x01u) == 0) {
        return;
    }

    u8 scancode = inb(PS2_DATA);
    if ((status & 0x20u) == 0) {
        int c = ps2_translate_scancode(scancode);
        if (c >= 0) {
            keyboard_enqueue((char)c);
        }
    }
}

char kernel_get_char(void)
{
    return keyboard_getchar();
}

void kernel_read_line(char *dest_buf, uint32_t max_len)
{
    if (dest_buf == 0 || max_len == 0) {
        return;
    }

    uint32_t length = 0;
    dest_buf[0] = '\0';

    for (;;) {
        char c = keyboard_getchar();
        
        // Terminals send \r (Carriage Return) on Enter. Handle it and move to next line.
        if (c == '\n' || c == '\r') {
            serial_put_char('\r');
            serial_put_char('\n');
            break;
        }
        
        // Handle Backspace / Delete keys
        if (c == '\b' || (uint8_t)c == 0x7Fu) {
            if (length != 0) {
                --length;
                dest_buf[length] = '\0';
                
                // Clear the character visually on the terminal screen
                serial_put_char('\b');
                serial_put_char(' ');
                serial_put_char('\b');
            }
            continue;
        }
        
        // Accept printable characters
        if ((c >= ' ' || c == '\t') && length < max_len - 1u) {
            serial_put_char(c); // ECHO: Print it immediately as they type!
            dest_buf[length++] = c;
            dest_buf[length] = '\0';
        }
    }

    dest_buf[length] = '\0';
}

int keyboard_poll_char(void)
{
    u32 flags = keyboard_irq_save();
    int c = keyboard_dequeue();
    keyboard_irq_restore(flags);
    if (c >= 0) {
        return c;
    }

    if (serial_data_ready()) {
        return (unsigned char)inb(COM1_DATA);
    }
    return -1;
}

char keyboard_getchar(void)
{
    u32 flags = keyboard_irq_save();
    for (;;) {
        int c = keyboard_dequeue();
        if (c < 0 && serial_data_ready()) {
            c = (unsigned char)inb(COM1_DATA);
        }
        if (c >= 0) {
            keyboard_irq_restore(flags);
            return (char)c;
        }

        asm volatile("sti; hlt; cli" : : : "memory");
    }
}
