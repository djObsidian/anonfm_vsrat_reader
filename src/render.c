#include "render.h"
#include "colors.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>

#define AFM_RESET   "\033[0m"
#define AFM_RED_FG  "\033[91m"

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
 * line is appended right after the header label (no leading newline). When
 * color_idx >= 0, every line is wrapped in a 256-colour foreground sequence
 * and a reset, so the colour does not bleed past the body or into the next
 * stanza after a hard wrap. */
static void afm_print_body(const char *body, int color_idx)
{
    const char *open  = "";
    const char *close = "";
    char        open_buf[24];

    if (color_idx >= 0) {
        snprintf(open_buf, sizeof(open_buf), "\033[38;5;%dm", color_idx);
        open  = open_buf;
        close = AFM_RESET;
    }

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
            fputs(open, stdout);
            fwrite(p, 1, n, stdout);
            fputs(close, stdout);
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
    int  color_on        = afm_color_globally_enabled();
    int  dj_color        = afm_color_for_dj(e->dj);
    int  list_color      = afm_color_for_listener(e->listener);
    int  dj_text_color   = afm_color_for_dj_text(e->dj);
    int  list_text_color = afm_color_for_listener_text(e->listener);
    const char *red = color_on ? AFM_RED_FG : "";
    const char *rst = color_on ? AFM_RESET  : "";

    if (with_separator) afm_print_separator();

    if (e->announce) {
        fprintf(stdout, "%s[ОБЪЯВЛЕНИЕ]!%s %s%s%s: ", red, rst, red, e->ts, rst);
        afm_print_nick(e->dj, dj_color);
        fputs(": ", stdout);
        afm_print_body(e->dj_resp, dj_text_color);
        return;
    }

    /* Listener stanza */
    fprintf(stdout, "%s%s%s: ", red, e->ts, rst);
    afm_print_nick(e->listener, list_color);
    fputs(": ", stdout);
    afm_print_body(e->listener_msg, list_text_color);

    /* DJ stanza */
    afm_print_nick(e->dj, dj_color);
    fputs(": ", stdout);
    afm_print_body(e->dj_resp, dj_text_color);
}
