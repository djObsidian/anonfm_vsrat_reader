#include "colors.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Built-in fallback palettes (xterm-256 indices). Listeners stay cool;
 * DJs span the warm + bright spectrum so two on-air djs are unlikely to
 * collide. Avoid colours that are also in the listener palette so a DJ
 * never visually impersonates a listener. */
static const int afm_listener_palette[] = {
    51, 87, 159, 195,                    /* cyans / pale blues */
    207, 213, 219, 225,                  /* pinks / magentas */
    105, 111, 117, 123,                  /* purpleish */
    140, 141, 146, 153
};
static const int afm_dj_palette[] = {
    196, 202, 208, 214,                  /* red → orange */
    220, 226, 228, 222,                  /* yellow */
    46,  82,  118, 154, 190,             /* bright greens */
    119, 156,                            /* pale greens */
    45,  50,  81,                        /* cyan (distinct from listener) */
    33,  39,  75,                        /* blue */
    99,  135, 171, 177,                  /* purple / violet */
    198, 199, 200, 209                   /* pink / magenta */
};
#define AFM_LISTENER_PALETTE_N \
    ((int)(sizeof(afm_listener_palette) / sizeof(afm_listener_palette[0])))
#define AFM_DJ_PALETTE_N \
    ((int)(sizeof(afm_dj_palette) / sizeof(afm_dj_palette[0])))

/* Sentinel rule values. 0..255 = a concrete xterm-256 index.
 *   -1  = "off" — do not colour
 *   -2  = "hash" — pick from the built-in palette by FNV-1a(nick) */
#define AFM_RULE_OFF  (-1)
#define AFM_RULE_HASH (-2)

struct afm_color_rule {
    char *nick;       /* NULL means default ('*') */
    int   color;      /* AFM_RULE_OFF, AFM_RULE_HASH, or 0..255 */
};

struct afm_color_table {
    struct afm_color_rule *items;
    size_t                 count;
    size_t                 cap;
    int                    default_color;
};

/* Nick-badge tables default to `hash` (any nick gets a colour). */
static struct afm_color_table g_listeners      = { NULL, 0, 0, AFM_RULE_HASH };
static struct afm_color_table g_djs            = { NULL, 0, 0, AFM_RULE_HASH };
/* Body-text tables default to `off` — bodies are uncoloured unless the
 * config explicitly opts in. */
static struct afm_color_table g_listener_text  = { NULL, 0, 0, AFM_RULE_OFF };
static struct afm_color_table g_dj_text        = { NULL, 0, 0, AFM_RULE_OFF };
static int                    g_disabled       = 0;

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
    if (strcmp(tok, "hash") == 0) { *out = AFM_RULE_HASH; return 0; }
    if (strcmp(tok, "off")  == 0) { *out = AFM_RULE_OFF;  return 0; }
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
    } else if (strcmp(kind, "listener_text") == 0) {
        return afm_table_add(&g_listener_text, nick, color);
    } else if (strcmp(kind, "dj_text") == 0) {
        return afm_table_add(&g_dj_text, nick, color);
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
    afm_table_free(&g_listener_text);
    afm_table_free(&g_dj_text);
    g_listeners.default_color     = AFM_RULE_HASH;
    g_djs.default_color           = AFM_RULE_HASH;
    g_listener_text.default_color = AFM_RULE_OFF;
    g_dj_text.default_color       = AFM_RULE_OFF;

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

int afm_color_globally_enabled(void)
{
    return g_disabled ? 0 : 1;
}

/* Resolve a rule (-1/-2/0..255) to a concrete xterm-256 index, or -1 if
 * the rule says "off" or colour is globally disabled. */
static int afm_resolve(int rule, const char *nick,
                       const int *palette, int palette_n)
{
    if (rule == AFM_RULE_OFF) return -1;
    if (rule == AFM_RULE_HASH) {
        unsigned long h = afm_hash_nick(nick);
        return palette[h % (unsigned long)palette_n];
    }
    return rule;
}

int afm_color_for_listener(const char *nick)
{
    if (g_disabled || nick == NULL) return -1;
    return afm_resolve(afm_lookup(&g_listeners, nick), nick,
                       afm_listener_palette, AFM_LISTENER_PALETTE_N);
}

int afm_color_for_dj(const char *nick)
{
    if (g_disabled || nick == NULL) return -1;
    return afm_resolve(afm_lookup(&g_djs, nick), nick,
                       afm_dj_palette, AFM_DJ_PALETTE_N);
}

int afm_color_for_listener_text(const char *nick)
{
    if (g_disabled || nick == NULL) return -1;
    return afm_resolve(afm_lookup(&g_listener_text, nick), nick,
                       afm_listener_palette, AFM_LISTENER_PALETTE_N);
}

int afm_color_for_dj_text(const char *nick)
{
    if (g_disabled || nick == NULL) return -1;
    return afm_resolve(afm_lookup(&g_dj_text, nick), nick,
                       afm_dj_palette, AFM_DJ_PALETTE_N);
}
