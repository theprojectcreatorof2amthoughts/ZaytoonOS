#include "kernel/drivers/keyboard.h"
#include "kernel/arch/io.h"

#define COM1_BASE       0x3F8u
#define COM1_DATA       (COM1_BASE + 0u)
#define COM1_INTERRUPT (COM1_BASE + 1u)
#define COM1_FIFO       (COM1_BASE + 2u)
#define COM1_LINE       (COM1_BASE + 3u)
#define COM1_MODEM      (COM1_BASE + 4u)
#define COM1_STATUS     (COM1_BASE + 5u)

#define UART_LCR_DLAB   0x80u
#define UART_LCR_8N1    0x03u
#define UART_FIFO_INIT  0xC7u
#define UART_LSR_DATA   0x01u
#define UART_LSR_THRE   0x20u  // Transmitter Holding Register Empty flag

static int serial_data_ready(void)
{
    return (inb(COM1_STATUS) & UART_LSR_DATA) != 0;
}

/* Check if the serial port is clear to send data back to the screen */
static int serial_transmit_ready(void)
{
    return (inb(COM1_STATUS) & UART_LSR_THRE) != 0;
}

/* Transmit a character back to the console window (Crucial for Local Echo) */
static void serial_put_char(char c)
{
    while (!serial_transmit_ready()) {
        asm volatile("pause");
    }
    outb(COM1_DATA, (uint8_t)c);
}

void keyboard_init(void)
{
    // 1. Disable hardware interrupts entirely during initial setup
    outb(COM1_INTERRUPT, 0x00u);
    
    // 2. Turn on DLAB to configure the clock baud rate
    outb(COM1_LINE, UART_LCR_DLAB);
    
    // 3. Set the baud rate divisor to 3 (38400 Baud)
    outb(COM1_DATA, 0x03u);
    outb(COM1_INTERRUPT, 0x00u);
    
    // 4. Turn off DLAB, lock mode to 8 bits, no parity, 1 stop bit (8N1)
    outb(COM1_LINE, UART_LCR_8N1);
    
    // 5. Enable FIFO, clear both the receiver and transmitter queues
    outb(COM1_FIFO, UART_FIFO_INIT);
    
    // 6. Set Auxiliary Output 2 (0x08) to allow hardware signals to flow 
    // and assert RTS/DTR lines (0x01 | 0x02) -> 0x0B total.
    outb(COM1_MODEM, 0x0Bu);
    
    // 7. CRUCIAL FOR v86: Flush any garbage boot bytes from the registers
    // to clear the internal emulator latch before we start polling!
    (void)inb(COM1_STATUS);
    (void)inb(COM1_DATA);
}


char kernel_get_char(void)
{
    while (!serial_data_ready()) {
        asm volatile("pause");
    }
    return (char)inb(COM1_DATA);
}

void kernel_read_line(char *dest_buf, uint32_t max_len)
{
    if (dest_buf == 0 || max_len == 0) {
        return;
    }

    uint32_t length = 0;
    dest_buf[0] = '\0';

    for (;;) {
        char c = kernel_get_char();
        
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
    if (!serial_data_ready()) {
        return -1;
    }

    char c = (char)inb(COM1_DATA);
    
    // Automatically perform local echo even for legacy applications calling the polling handler
    if (c == '\r' || c == '\n') {
        serial_put_char('\r');
        serial_put_char('\n');
        return '\n';
    }
    if ((unsigned char)c == 0x7Fu || c == '\b') {
        serial_put_char('\b');
        serial_put_char(' ');
        serial_put_char('\b');
        return '\b';
    }
    
    serial_put_char(c); // Echo character back out
    return (unsigned char)c;
}

char keyboard_getchar(void)
{
    int c;
    while ((c = keyboard_poll_char()) < 0) {
        asm volatile("pause");
    }
    return (char)c;
}
