#include "platform.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#  ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#  endif
#else
#  include <unistd.h>
#  include <sys/ioctl.h>
#  include <time.h>
#endif

/* 1 = ANSI escapes work (POSIX or Win10+ ConPTY), 0 = need WinAPI. */
static int g_ansi_ok = 1;
/* 1 = stdout is a Windows console handle and we should write to it via
 * WriteConsoleW (with UTF-8 → UTF-16 conversion). 0 = fwrite is fine. */
static int g_use_wide_console = 0;

#ifdef _WIN32
/* Saved attributes captured at startup so afm_style_reset can restore the
 * user's original cmd.exe colour scheme (typically light-grey on black,
 * but we don't assume). */
static WORD g_saved_attrs = 0x07;  /* sane default: light-grey on black */
#endif

void afm_console_init(void)
{
#ifdef _WIN32
    HANDLE h_out;
    HANDLE h_err;
    DWORD  mode;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int    vt_ok       = 0;
    int    is_console  = 0;

    /* Codepage matters only when stdout is redirected to a console-based
     * tool that DOES read bytes (rare). For our actual console writes we
     * use WriteConsoleW below, which is codepage-independent. Setting
     * 65001 anyway, in case g_use_wide_console ends up false. */
    SetConsoleOutputCP(CP_UTF8);

    h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h_out != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(h_out, &csbi)) {
        g_saved_attrs = csbi.wAttributes;
        is_console    = 1;
    }
    if (h_out != INVALID_HANDLE_VALUE && GetConsoleMode(h_out, &mode)) {
        is_console = 1;
        if (SetConsoleMode(h_out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            vt_ok = 1;
        }
    }
    h_err = GetStdHandle(STD_ERROR_HANDLE);
    if (h_err != INVALID_HANDLE_VALUE && GetConsoleMode(h_err, &mode)) {
        SetConsoleMode(h_err, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    /* If VT could not be enabled (Win7 cmd.exe, older terminals), drop to
     * legacy attribute-based colouring. Output redirected to a file gets
     * GetConsoleMode == 0; treat that as "ANSI is fine" — escapes in a
     * file are normal. */
    g_ansi_ok = vt_ok || !is_console;
    /* Use the WriteConsoleW path whenever stdout points at a console.
     * That fixes the Win7 codepage chaos: even with CP=65001, cmd.exe
     * writes UTF-8 byte-by-byte and splits multi-byte sequences, so
     * Cyrillic comes out as CP866 line-drawing soup. WriteConsoleW takes
     * UTF-16 wide chars directly and bypasses that whole pipeline. */
    g_use_wide_console = is_console;
#else
    g_ansi_ok = 1;
    g_use_wide_console = 0;
#endif
}

int afm_console_supports_ansi(void)
{
    return g_ansi_ok;
}

#ifdef _WIN32
/* Convert xterm-256 index to (R, G, B) in [0, 255]. Mirrors xterm's
 * standard mapping: 0..15 = system, 16..231 = 6x6x6 cube, 232..255 = grey. */
static void afm_xterm_to_rgb(int idx, int *r, int *g, int *b)
{
    /* System CGA colours, RGB triples consistent with classic Windows
     * console palette (which is what we'll target on Win7). */
    static const unsigned char sys_rgb[16][3] = {
        {  0,   0,   0}, {  0,   0, 170}, {  0, 170,   0}, {  0, 170, 170},
        {170,   0,   0}, {170,   0, 170}, {170,  85,   0}, {170, 170, 170},
        { 85,  85,  85}, { 85,  85, 255}, { 85, 255,  85}, { 85, 255, 255},
        {255,  85,  85}, {255,  85, 255}, {255, 255,  85}, {255, 255, 255}
    };
    static const unsigned char cube[6] = { 0, 95, 135, 175, 215, 255 };

    if (idx < 0)        { *r = *g = *b = 0; return; }
    if (idx < 16) {
        *r = sys_rgb[idx][0]; *g = sys_rgb[idx][1]; *b = sys_rgb[idx][2];
    } else if (idx < 232) {
        int n = idx - 16;
        *r = cube[(n / 36) % 6];
        *g = cube[(n /  6) % 6];
        *b = cube[ n       % 6];
    } else if (idx < 256) {
        int v = 8 + (idx - 232) * 10;
        *r = *g = *b = v;
    } else {
        *r = *g = *b = 0;
    }
}

/* Closest of the 16 Windows console colours, as an attribute nibble. */
static WORD afm_xterm_to_winattr_nibble(int idx)
{
    /* RGB of the 16 console attributes, in attribute index order. */
    static const unsigned char attr_rgb[16][3] = {
        {  0,   0,   0},  /* 0 BLACK */
        {  0,   0, 170},  /* 1 BLUE */
        {  0, 170,   0},  /* 2 GREEN */
        {  0, 170, 170},  /* 3 CYAN */
        {170,   0,   0},  /* 4 RED */
        {170,   0, 170},  /* 5 MAGENTA */
        {170,  85,   0},  /* 6 YELLOW (brown) */
        {170, 170, 170},  /* 7 LIGHTGRAY */
        { 85,  85,  85},  /* 8 DARKGRAY (intense black) */
        { 85,  85, 255},  /* 9 LIGHTBLUE */
        { 85, 255,  85},  /* 10 LIGHTGREEN */
        { 85, 255, 255},  /* 11 LIGHTCYAN */
        {255,  85,  85},  /* 12 LIGHTRED */
        {255,  85, 255},  /* 13 LIGHTMAGENTA */
        {255, 255,  85},  /* 14 YELLOW (light) */
        {255, 255, 255}   /* 15 WHITE */
    };
    int  r, g, b;
    int  best_i = 7;
    long best_d = 0x7FFFFFFFL;
    int  i;
    afm_xterm_to_rgb(idx, &r, &g, &b);
    for (i = 0; i < 16; ++i) {
        long dr = (long)attr_rgb[i][0] - r;
        long dg = (long)attr_rgb[i][1] - g;
        long db = (long)attr_rgb[i][2] - b;
        long d  = dr * dr + dg * dg + db * db;
        if (d < best_d) { best_d = d; best_i = i; }
    }
    return (WORD)best_i;
}

/* Compose final WORD attribute. fg/bg of -1 means "keep saved attribute".
 * The Windows attribute byte is: bg (high nibble) | fg (low nibble),
 * each nibble = R(1) | G(2) | B(4) | INTENSITY(8) — already encoded by
 * afm_xterm_to_winattr_nibble. */
static WORD afm_compose_attr(int fg256, int bg256)
{
    WORD fg_nib = (WORD)(g_saved_attrs & 0x0F);
    WORD bg_nib = (WORD)((g_saved_attrs >> 4) & 0x0F);
    if (fg256 >= 0) fg_nib = afm_xterm_to_winattr_nibble(fg256);
    if (bg256 >= 0) bg_nib = afm_xterm_to_winattr_nibble(bg256);
    return (WORD)((bg_nib << 4) | fg_nib);
}
#endif

/* ---------- Unified write API ----------------------------------------- */

void afm_print_bytes(const char *bytes, size_t n)
{
    if (n == 0 || bytes == NULL) return;
#ifdef _WIN32
    if (g_use_wide_console) {
        HANDLE  h    = GetStdHandle(STD_OUTPUT_HANDLE);
        int     wlen;
        wchar_t stack_wbuf[1024];
        wchar_t *wbuf = stack_wbuf;
        DWORD   wrote = 0;
        if (h == INVALID_HANDLE_VALUE) {
            fwrite(bytes, 1, n, stdout);
            return;
        }
        if (n > (size_t)0x7FFFFFFE) {
            /* Pathological — chunk it. */
            afm_print_bytes(bytes, n / 2);
            afm_print_bytes(bytes + n / 2, n - n / 2);
            return;
        }
        wlen = MultiByteToWideChar(CP_UTF8, 0, bytes, (int)n, NULL, 0);
        if (wlen <= 0) {
            /* Not valid UTF-8 — fall back so we at least get *something*. */
            fflush(stdout);
            WriteFile(h, bytes, (DWORD)n, &wrote, NULL);
            return;
        }
        if ((size_t)wlen > sizeof(stack_wbuf) / sizeof(stack_wbuf[0])) {
            wbuf = (wchar_t *)malloc(sizeof(wchar_t) * (size_t)wlen);
            if (wbuf == NULL) {
                fwrite(bytes, 1, n, stdout);
                return;
            }
        }
        MultiByteToWideChar(CP_UTF8, 0, bytes, (int)n, wbuf, wlen);
        WriteConsoleW(h, wbuf, (DWORD)wlen, &wrote, NULL);
        if (wbuf != stack_wbuf) free(wbuf);
        return;
    }
#endif
    fwrite(bytes, 1, n, stdout);
}

void afm_print_str(const char *s)
{
    if (s != NULL) afm_print_bytes(s, strlen(s));
}

void afm_print_chr(char c)
{
    afm_print_bytes(&c, 1);
}

void afm_print_fmt(const char *fmt, ...)
{
    char    stack_buf[1024];
    char   *buf = stack_buf;
    va_list ap;
    int     n;

    va_start(ap, fmt);
    n = vsnprintf(stack_buf, sizeof(stack_buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof(stack_buf)) {
        size_t need = (size_t)n + 1;
        buf = (char *)malloc(need);
        if (buf == NULL) {
            afm_print_bytes(stack_buf, sizeof(stack_buf) - 1);
            return;
        }
        va_start(ap, fmt);
        vsnprintf(buf, need, fmt, ap);
        va_end(ap);
    }
    afm_print_bytes(buf, (size_t)n);
    if (buf != stack_buf) free(buf);
}

void afm_console_flush(void)
{
    fflush(stdout);
}

/* ---------- Styled output -------------------------------------------- */

void afm_style_apply(int fg_xterm256, int bg_xterm256)
{
    if (g_ansi_ok) {
        if (fg_xterm256 >= 0 && bg_xterm256 >= 0) {
            afm_print_fmt("\033[38;5;%d;48;5;%dm", fg_xterm256, bg_xterm256);
        } else if (fg_xterm256 >= 0) {
            afm_print_fmt("\033[38;5;%dm", fg_xterm256);
        } else if (bg_xterm256 >= 0) {
            afm_print_fmt("\033[48;5;%dm", bg_xterm256);
        }
        return;
    }
#ifdef _WIN32
    {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE) return;
        fflush(stdout);
        SetConsoleTextAttribute(h, afm_compose_attr(fg_xterm256, bg_xterm256));
    }
#else
    (void)fg_xterm256; (void)bg_xterm256;
#endif
}

void afm_style_reset(void)
{
    if (g_ansi_ok) {
        afm_print_str("\033[0m");
        return;
    }
#ifdef _WIN32
    {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE) return;
        fflush(stdout);
        SetConsoleTextAttribute(h, g_saved_attrs);
    }
#endif
}

int afm_term_width(void)
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h_out != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(h_out, &info)) {
        int w = (int)(info.srWindow.Right - info.srWindow.Left + 1);
        if (w > 0) return w;
    }
    return 80;
#else
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return (int)ws.ws_col;
    }
    return 80;
#endif
}

void afm_sleep_ms(unsigned int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec  = (time_t)(ms / 1000U);
    ts.tv_nsec = (long)((ms % 1000U) * 1000000UL);
    nanosleep(&ts, NULL);
#endif
}

int afm_default_color_config_paths(char *out_buf, size_t out_buf_len)
{
    /* Candidates, in order:
     *   ./anonfm_colors.conf
     *   $XDG_CONFIG_HOME/anonfm/colors.conf  (or %APPDATA%\anonfm\colors.conf)
     *   $HOME/.config/anonfm/colors.conf      (POSIX only)
     */
    int    count   = 0;
    size_t pos     = 0;
    size_t needed;
    const char *cwd_rel = "anonfm_colors.conf";
    const char *home;
    const char *xdg;
#ifdef _WIN32
    const char *appdata;
#endif

    if (out_buf == NULL || out_buf_len == 0) return 0;

    needed = strlen(cwd_rel) + 1;
    if (pos + needed + 1 < out_buf_len) {
        memcpy(out_buf + pos, cwd_rel, needed);
        pos += needed;
        ++count;
    }

#ifdef _WIN32
    appdata = getenv("APPDATA");
    if (appdata != NULL && *appdata != '\0') {
        char tmp[1024];
        int n = _snprintf(tmp, sizeof(tmp), "%s\\anonfm\\colors.conf", appdata);
        if (n > 0 && (size_t)n < sizeof(tmp)) {
            needed = (size_t)n + 1;
            if (pos + needed + 1 < out_buf_len) {
                memcpy(out_buf + pos, tmp, needed);
                pos += needed;
                ++count;
            }
        }
    }
    (void)home; (void)xdg;
#else
    xdg = getenv("XDG_CONFIG_HOME");
    if (xdg != NULL && *xdg != '\0') {
        char tmp[1024];
        int n = snprintf(tmp, sizeof(tmp), "%s/anonfm/colors.conf", xdg);
        if (n > 0 && (size_t)n < sizeof(tmp)) {
            needed = (size_t)n + 1;
            if (pos + needed + 1 < out_buf_len) {
                memcpy(out_buf + pos, tmp, needed);
                pos += needed;
                ++count;
            }
        }
    }
    home = getenv("HOME");
    if (home != NULL && *home != '\0') {
        char tmp[1024];
        int n = snprintf(tmp, sizeof(tmp), "%s/.config/anonfm/colors.conf", home);
        if (n > 0 && (size_t)n < sizeof(tmp)) {
            needed = (size_t)n + 1;
            if (pos + needed + 1 < out_buf_len) {
                memcpy(out_buf + pos, tmp, needed);
                pos += needed;
                ++count;
            }
        }
    }
#endif

    /* Final terminator (a second NUL after last path). */
    if (pos < out_buf_len) {
        out_buf[pos] = '\0';
    } else {
        out_buf[out_buf_len - 1] = '\0';
    }
    return count;
}
