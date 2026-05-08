#include "platform.h"

#include <stdio.h>
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

void afm_console_init(void)
{
#ifdef _WIN32
    HANDLE h_out;
    HANDLE h_err;
    DWORD mode;

    SetConsoleOutputCP(CP_UTF8);

    h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h_out != INVALID_HANDLE_VALUE && GetConsoleMode(h_out, &mode)) {
        SetConsoleMode(h_out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    h_err = GetStdHandle(STD_ERROR_HANDLE);
    if (h_err != INVALID_HANDLE_VALUE && GetConsoleMode(h_err, &mode)) {
        SetConsoleMode(h_err, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
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
