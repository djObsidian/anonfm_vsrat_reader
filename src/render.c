#include "render.h"
#include "colors.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>

#define AFM_RESET   "\033[0m"
#define AFM_RED_FG  "\033[91m"

static int afm_color_enabled(void)
{
    /* Cheap probe: if the listener of "anything" returns -1, colour is off.
     * (afm_colors_disable() flips a global flag.) */
    return afm_color_for_listener("__probe__") >= 0 ? 1 : 0;
}

/* Print a 256-color background block with contrasting black foreground for
 * the nick. Falls back to plain text when colour is disabled. */
static void afm_print_nick(const char *nick, int color_idx)
{
    if (color_idx < 0) {
        fputs(nick, stdout);
        return;
    }
    /* xterm-256 background + black foreground for readability. */
    fprintf(stdout, "\033[38;5;0;48;5;%dm%s%s", color_idx, nick, AFM_RESET);
}

/* Print body text. Split on '\n' so each line starts at column 0; the first
 * line is appended right after the header label (no leading newline). */
static void afm_print_body(const char *body)
{
    if (body == NULL || *body == '\0') {
        fputc('\n', stdout);
        return;
    }
    {
        const char *p     = body;
        int         first = 1;
        while (*p != '\0') {
            const char *eol = strchr(p, '\n');
            size_t      n   = (eol == NULL) ? strlen(p) : (size_t)(eol - p);
            if (!first) fputc('\n', stdout);
            fwrite(p, 1, n, stdout);
            first = 0;
            if (eol == NULL) break;
            p = eol + 1;
        }
        fputc('\n', stdout);
    }
}

static void afm_print_separator(void)
{
    int width = afm_term_width();
    int i;
    /* Use ASCII '-' to keep things universally renderable. */
    for (i = 0; i < width; ++i) fputc('-', stdout);
    fputc('\n', stdout);
}

void afm_render_entry(const struct afm_entry *e, int with_separator)
{
    int  color_on   = afm_color_enabled();
    int  dj_color   = afm_color_for_dj(e->dj);
    int  list_color = afm_color_for_listener(e->listener);
    const char *red = color_on ? AFM_RED_FG : "";
    const char *rst = color_on ? AFM_RESET  : "";

    if (with_separator) afm_print_separator();

    if (e->announce) {
        fprintf(stdout, "%s[ОБЪЯВЛЕНИЕ]!%s %s%s%s: ", red, rst, red, e->ts, rst);
        afm_print_nick(e->dj, dj_color);
        fputs(": ", stdout);
        afm_print_body(e->dj_resp);
        return;
    }

    /* Listener stanza */
    fprintf(stdout, "%s%s%s: ", red, e->ts, rst);
    afm_print_nick(e->listener, list_color);
    fputs(": ", stdout);
    afm_print_body(e->listener_msg);

    /* DJ stanza */
    afm_print_nick(e->dj, dj_color);
    fputs(": ", stdout);
    afm_print_body(e->dj_resp);
}
