#ifndef ZAYTOONOS_KEYBOARD_H
#define ZAYTOONOS_KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
char kernel_get_char(void);
void kernel_read_line(char *dest_buf, uint32_t max_len);

/* Compatibility APIs used by existing in-kernel applications. */
char keyboard_getchar(void);
int keyboard_poll_char(void);

#endif
