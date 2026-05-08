#include "colors.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Built-in fallback palettes (xterm-256 indices). Listener tones go cool,
 * DJ tones go warm, broadly mirroring the reference screenshot. */
static const int afm_listener_palette[] = {
    51, 87, 159, 195,                    /* cyans / pale blues */
    207, 213, 219, 225,                  /* pinks / magentas */
    105, 111, 117, 123,                  /* purpleish */
    140, 141, 146, 153
};
static const int afm_dj_palette[] = {
    220, 221, 222, 226, 227, 228, 229,   /* yellows */
    118, 119, 120, 121, 154, 155, 156    /* greens */
};
#define AFM_LISTENER_PALETTE_N \
    ((int)(sizeof(afm_listener_palette) / sizeof(afm_listener_palette[0])))
#define AFM_DJ_PALETTE_N \
    ((int)(sizeof(afm_dj_palette) / sizeof(afm_dj_palette[0])))

struct afm_color_rule {
    char *nick;       /* NULL means default ('*') */
    int   color;      /* -2 = "hash", otherwise 0..255 */
};

struct afm_color_table {
    struct afm_color_rule *items;
    size_t                 count;
    size_t                 cap;
    int                    default_color;  /* -2 = hash */
};

static struct afm_color_table g_listeners = { NULL, 0, 0, -2 };
static struct afm_color_table g_djs       = { NULL, 0, 0, -2 };
static int                    g_disabled  = 0;

static unsigned long afm_hash_nick(const char *nick)
{
    unsigned long h = 2166136261UL;
    while (*nick != '\0') {
        h ^= (unsigned char)*nick++;
        h *= 16777619UL;
        h &= 0xFFFFFFFFUL;
    }
    return h;
}

static int afm_table_add(struct afm_color_table *t, const char *nick, int color)
{
    if (t->count >= t->cap) {
        size_t new_cap = t->cap == 0 ? 8 : t->cap * 2;
        struct afm_color_rule *p =
            (struct afm_color_rule *)realloc(t->items,
                                             sizeof(*p) * new_cap);
        if (p == NULL) return -1;
        t->items = p;
        t->cap   = new_cap;
    }
    if (nick == NULL || strcmp(nick, "*") == 0) {
        t->default_color = color;
        return 0;
    }
    t->items[t->count].nick  = strdup(nick);
    if (t->items[t->count].nick == NULL) return -1;
    t->items[t->count].color = color;
    ++t->count;
    return 0;
}

static int afm_lookup(const struct afm_color_table *t, const char *nick)
{
    size_t i;
    for (i = 0; i < t->count; ++i) {
        if (strcmp(t->items[i].nick, nick) == 0) return t->items[i].color;
    }
    return t->default_color;
}

static void afm_table_free(struct afm_color_table *t)
{
    size_t i;
    for (i = 0; i < t->count; ++i) free(t->items[i].nick);
    free(t->items);
    t->items = NULL;
    t->count = 0;
    t->cap   = 0;
}

static int afm_parse_color_token(const char *tok, int *out)
{
    if (strcmp(tok, "hash") == 0) { *out = -2; return 0; }
    {
        char *endp;
        long  v = strtol(tok, &endp, 10);
        if (*endp != '\0' || v < 0 || v > 255) return -1;
        *out = (int)v;
        return 0;
    }
}

/* Parse a single line in-place. Whitespace-separated:
 *   <kind> <nick> <color>
 * '#' starts a comment. '*' nick = default. */
static int afm_parse_line(char *line)
{
    char *p   = line;
    char *kind;
    char *nick;
    char *color_tok;
    int   color;

    /* strip comment */
    {
        char *hash = strchr(p, '#');
        if (hash != NULL) *hash = '\0';
    }

    while (*p != '\0' && isspace((unsigned char)*p)) ++p;
    if (*p == '\0') return 0;

    kind = p;
    while (*p != '\0' && !isspace((unsigned char)*p)) ++p;
    if (*p == '\0') return -1;
    *p++ = '\0';

    while (*p != '\0' && isspace((unsigned char)*p)) ++p;
    if (*p == '\0') return -1;
    nick = p;
    while (*p != '\0' && !isspace((unsigned char)*p)) ++p;
    if (*p == '\0') return -1;
    *p++ = '\0';

    while (*p != '\0' && isspace((unsigned char)*p)) ++p;
    if (*p == '\0') return -1;
    color_tok = p;
    while (*p != '\0' && !isspace((unsigned char)*p)) ++p;
    *p = '\0';

    if (afm_parse_color_token(color_tok, &color) != 0) return -1;

    if (strcmp(kind, "listener") == 0) {
        return afm_table_add(&g_listeners, nick, color);
    } else if (strcmp(kind, "dj") == 0) {
        return afm_table_add(&g_djs, nick, color);
    }
    return -1;
}

static int afm_load_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    char  line[1024];
    int   line_no = 0;
    int   ok      = 0;
    if (f == NULL) return -1;
    while (fgets(line, sizeof(line), f) != NULL) {
        ++line_no;
        /* trim trailing newline */
        {
            size_t L = strlen(line);
            while (L > 0
                   && (line[L - 1] == '\n' || line[L - 1] == '\r'
                       || line[L - 1] == ' ' || line[L - 1] == '\t')) {
                line[--L] = '\0';
            }
        }
        if (afm_parse_line(line) != 0) {
            fprintf(stderr, "colors: %s:%d: bad line\n", path, line_no);
        } else {
            ok = 1;
        }
    }
    fclose(f);
    return ok ? 0 : -1;
}

void afm_colors_init(const char *explicit_path)
{
    /* Reset tables in case called twice. */
    afm_table_free(&g_listeners);
    afm_table_free(&g_djs);
    g_listeners.default_color = -2;
    g_djs.default_color       = -2;

    if (explicit_path != NULL) {
        if (afm_load_file(explicit_path) != 0) {
            fprintf(stderr, "colors: failed to load %s\n", explicit_path);
        }
        return;
    }

    {
        char path_buf[4096];
        int  count = afm_default_color_config_paths(path_buf, sizeof(path_buf));
        const char *p = path_buf;
        int  i;
        for (i = 0; i < count; ++i) {
            if (afm_load_file(p) == 0) return;  /* first hit wins */
            p += strlen(p) + 1;
        }
    }
    /* No config — pure hash mode. */
}

void afm_colors_disable(void)
{
    g_disabled = 1;
}

int afm_color_for_listener(const char *nick)
{
    int rule;
    if (g_disabled || nick == NULL) return -1;
    rule = afm_lookup(&g_listeners, nick);
    if (rule == -2) {
        unsigned long h = afm_hash_nick(nick);
        return afm_listener_palette[h % (unsigned long)AFM_LISTENER_PALETTE_N];
    }
    return rule;
}

int afm_color_for_dj(const char *nick)
{
    int rule;
    if (g_disabled || nick == NULL) return -1;
    rule = afm_lookup(&g_djs, nick);
    if (rule == -2) {
        unsigned long h = afm_hash_nick(nick);
        return afm_dj_palette[h % (unsigned long)AFM_DJ_PALETTE_N];
    }
    return rule;
}
