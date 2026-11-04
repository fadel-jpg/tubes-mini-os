#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stddef.h>

#define KB_DATA_PORT   0x60
#define KB_STATUS_PORT 0x64

/* Initialize PS/2 keyboard state */
void keyboard_init(void);

/* Blocking read of a single ASCII character from PS/2 keyboard via polling */
char keyboard_getchar(void);

/* Read a full line of user input into buffer (handles Backspace & Enter) */
void keyboard_readline(char *buffer, size_t max_len);

#endif /* KEYBOARD_H */

