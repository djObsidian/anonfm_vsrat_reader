#include "render.h"
#include "colors.h"
#include "platform.h"

#include <string.h>

/* xterm-256 indices used by the static parts of the layout. */
#define AFM_RED_FG_IDX  9   /* bright red — keeps the announce/timestamp pop */

/* Print a nick badge. The badge is rendered as black-on-color so it reads
 * like a tag. When colour is globally disabled or color_idx is -1, prints
 * the nick as plain text. */
static void afm_print_nick(const char *nick, int color_idx)
{
    if (color_idx < 0) {
        afm_print_str(nick);
        return;
    }
    afm_style_apply(0, color_idx);   /* black fg, palette bg */
    afm_print_str(nick);
    afm_style_reset();
}

/* Print body text. Split on '\n' so each line starts at column 0; the
 * first line is appended right after the header label (no leading
 * newline). When color_idx >= 0, every line is wrapped with apply/reset
 * so colour does not bleed past the body or into the next stanza after a
 * hard wrap (especially important on the WinAPI path, where a missing
 * reset would persist into the next prompt). */
static void afm_print_body(const char *body, int color_idx)
{
    int colour = (color_idx >= 0);
    if (body == NULL || *body == '\0') {
        afm_print_chr('\n');
        return;
    }
    {
        const char *p     = body;
        int         first = 1;
        while (*p != '\0') {
            const char *eol = strchr(p, '\n');
            size_t      n   = (eol == NULL) ? strlen(p) : (size_t)(eol - p);
            if (!first) afm_print_chr('\n');
            if (colour) afm_style_apply(color_idx, -1);
            afm_print_bytes(p, n);
            if (colour) afm_style_reset();
            first = 0;
            if (eol == NULL) break;
            p = eol + 1;
        }
        afm_print_chr('\n');
    }
}

static void afm_print_separator(void)
{
    int width = afm_term_width();
    int i;
    /* `\r` snaps the cursor to column 0 in case a previous body printed
     * exactly to the edge and Windows cmd left us mid-row.
     * `width - 1` dashes (not `width`): cmd.exe auto-wraps when the LAST
     * column is filled, then the explicit `\n` advances another row,
     * leaving an empty row between the rule and the next entry. One
     * column short avoids the auto-wrap entirely.
     * ASCII '-' so it renders identically everywhere — even on Win7 cmd
     * with a font that can't draw the U+2500 line-drawing characters. */
    afm_print_chr('\r');
    if (width < 2) width = 2;
    for (i = 0; i < width - 1; ++i) afm_print_chr('-');
    afm_print_chr('\n');
}

static void afm_print_red(const char *s)
{
    afm_style_apply(AFM_RED_FG_IDX, -1);
    afm_print_str(s);
    afm_style_reset();
}

void afm_render_entry(const struct afm_entry *e, int with_separator)
{
    int  color_on        = afm_color_globally_enabled();
    int  dj_color        = afm_color_for_dj(e->dj);
    int  list_color      = afm_color_for_listener(e->listener);
    int  dj_text_color   = afm_color_for_dj_text(e->dj);
    int  list_text_color = afm_color_for_listener_text(e->listener);

    if (with_separator) afm_print_separator();

    if (e->announce) {
        if (color_on) afm_print_red("[ОБЪЯВЛЕНИЕ]!");
        else          afm_print_str("[ОБЪЯВЛЕНИЕ]!");
        afm_print_chr(' ');
        if (color_on) afm_print_red(e->ts);
        else          afm_print_str(e->ts);
        afm_print_str(": ");
        afm_print_nick(e->dj, dj_color);
        afm_print_str(": ");
        afm_print_body(e->dj_resp, dj_text_color);
        return;
    }

    /* Listener stanza */
    if (color_on) afm_print_red(e->ts);
    else          afm_print_str(e->ts);
    afm_print_str(": ");
    afm_print_nick(e->listener, list_color);
    afm_print_str(": ");
    afm_print_body(e->listener_msg, list_text_color);

    /* DJ stanza */
    afm_print_nick(e->dj, dj_color);
    afm_print_str(": ");
    afm_print_body(e->dj_resp, dj_text_color);
}
