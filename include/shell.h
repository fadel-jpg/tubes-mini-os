#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>

#define SHELL_BUFFER_SIZE 128

/* Display the boot banner and ASCII logo */
void shell_print_banner(void);

/* Start the interactive CLI Read-Eval-Print Loop (REPL) */
void shell_run(void);

/* Execute a single command line string */
void shell_execute(const char *input);

/* Freestanding string utility functions */
size_t kstrlen(const char *str);
int kstrcmp(const char *s1, const char *s2);
int kstrncmp(const char *s1, const char *s2, size_t n);

#endif /* SHELL_H */

/*shell*/
