#ifndef AFM_PLATFORM_H
#define AFM_PLATFORM_H

#include <stddef.h>

/* Initialize the console for UTF-8 output and ANSI escape sequences.
 * No-op on POSIX. On Windows, switches output codepage to UTF-8 and
 * enables Virtual Terminal Processing on stdout/stderr. */
void afm_console_init(void);

/* Best-effort terminal width in characters; returns 80 if unknown. */
int afm_term_width(void);

/* Sleep for the given number of milliseconds. */
void afm_sleep_ms(unsigned int ms);

/* Resolve XDG-config-style path candidates for a colors config file.
 * Writes up to max_paths NUL-terminated absolute paths into out_buf, each
 * separated by '\0', terminated by an extra '\0'. Returns the count. */
int afm_default_color_config_paths(char *out_buf, size_t out_buf_len);

#endif
