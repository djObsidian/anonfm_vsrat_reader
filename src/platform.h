#ifndef AFM_PLATFORM_H
#define AFM_PLATFORM_H

#include <stddef.h>

/* Initialize the console for UTF-8 output and ANSI escape sequences.
 * No-op on POSIX. On Windows, switches output codepage to UTF-8 and tries
 * to enable Virtual Terminal Processing on stdout/stderr; if that fails
 * (Windows 7 / older CMD), the colour API below transparently falls back
 * to SetConsoleTextAttribute. */
void afm_console_init(void);

/* True (1) when colour can be done via ANSI escapes (Win10 ConPTY, any
 * POSIX TTY); false (0) when we have to drive the legacy WinAPI console
 * attributes API (Windows 7 cmd.exe). render.c uses afm_style_*() and
 * does not need to branch itself. */
int afm_console_supports_ansi(void);

/* Apply / reset text styling. Fg and bg are xterm-256 indices (0..255),
 * or -1 for "no change / default". On VT consoles emits CSI 38;5;n /
 * 48;5;n; on legacy Win7 CMD downconverts each to the nearest of the 16
 * console attributes and calls SetConsoleTextAttribute. */
void afm_style_apply(int fg_xterm256, int bg_xterm256);
void afm_style_reset(void);

/* Unified output API. ALL stdout writes from render.c MUST go through
 * these — direct fwrite/fputs would skip the UTF-8 → UTF-16 conversion
 * needed by Win7 cmd.exe (which splits multi-byte UTF-8 sequences across
 * single-byte WriteFile calls and renders Cyrillic as CP866 garbage even
 * with codepage 65001). On a Windows console, these route through
 * MultiByteToWideChar + WriteConsoleW, which renders correctly on every
 * Windows from XP onwards. On POSIX, on Win10 ConPTY, and when stdout is
 * redirected to a file/pipe, they fall through to fwrite. */
void afm_print_bytes(const char *bytes, size_t n);
void afm_print_str(const char *s);
void afm_print_chr(char c);
/* printf-style; format string is treated as ASCII, args may contain UTF-8. */
void afm_print_fmt(const char *fmt, ...);
/* Flush both the C stdio buffer and any pending console writes. */
void afm_console_flush(void);

/* Best-effort terminal width in characters; returns 80 if unknown. */
int afm_term_width(void);

/* Sleep for the given number of milliseconds. */
void afm_sleep_ms(unsigned int ms);

/* Resolve XDG-config-style path candidates for a colors config file.
 * Writes up to max_paths NUL-terminated absolute paths into out_buf, each
 * separated by '\0', terminated by an extra '\0'. Returns the count. */
int afm_default_color_config_paths(char *out_buf, size_t out_buf_len);

#endif
