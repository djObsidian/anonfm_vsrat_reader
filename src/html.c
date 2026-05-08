#include "html.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct afm_strbuf {
    char  *data;
    size_t len;
    size_t cap;
};

static int afm_sb_reserve(struct afm_strbuf *b, size_t add)
{
    size_t need = b->len + add + 1;
    if (need <= b->cap) return 0;
    {
        size_t new_cap = b->cap == 0 ? 256 : b->cap;
        char  *p;
        while (new_cap < need) new_cap *= 2;
        p = (char *)realloc(b->data, new_cap);
        if (p == NULL) return -1;
        b->data = p;
        b->cap  = new_cap;
    }
    return 0;
}

static int afm_sb_putc(struct afm_strbuf *b, char c)
{
    if (afm_sb_reserve(b, 1) != 0) return -1;
    b->data[b->len++] = c;
    b->data[b->len]   = '\0';
    return 0;
}

static int afm_sb_putn(struct afm_strbuf *b, const char *s, size_t n)
{
    if (afm_sb_reserve(b, n) != 0) return -1;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

/* Encode codepoint cp as UTF-8 bytes into buf (max 4 bytes), return length. */
static int afm_utf8_encode(unsigned long cp, char *buf)
{
    if (cp < 0x80UL) {
        buf[0] = (char)cp;
        return 1;
    } else if (cp < 0x800UL) {
        buf[0] = (char)(0xC0U | (cp >> 6));
        buf[1] = (char)(0x80U | (cp & 0x3FU));
        return 2;
    } else if (cp < 0x10000UL) {
        buf[0] = (char)(0xE0U | (cp >> 12));
        buf[1] = (char)(0x80U | ((cp >> 6) & 0x3FU));
        buf[2] = (char)(0x80U | (cp & 0x3FU));
        return 3;
    } else if (cp < 0x110000UL) {
        buf[0] = (char)(0xF0U | (cp >> 18));
        buf[1] = (char)(0x80U | ((cp >> 12) & 0x3FU));
        buf[2] = (char)(0x80U | ((cp >> 6) & 0x3FU));
        buf[3] = (char)(0x80U | (cp & 0x3FU));
        return 4;
    }
    /* invalid */
    buf[0] = '?';
    return 1;
}

struct afm_named_entity {
    const char   *name;
    unsigned long cp;
};

static const struct afm_named_entity afm_entities[] = {
    { "amp",    38UL },
    { "lt",     60UL },
    { "gt",     62UL },
    { "quot",   34UL },
    { "apos",   39UL },
    { "nbsp",  160UL },
    { "laquo", 171UL },
    { "raquo", 187UL },
    { "mdash", 8212UL },
    { "ndash", 8211UL },
    { "hellip",8230UL },
    { "ldquo", 8220UL },
    { "rdquo", 8221UL },
    { "lsquo", 8216UL },
    { "rsquo", 8217UL },
    { "copy",  169UL },
    { "reg",   174UL },
    { "trade", 8482UL },
    { NULL,    0UL }
};

/* Parse a single entity beginning right after '&'. *p_end gets the
 * pointer past the closing ';' (or after the malformed run). Returns
 * the codepoint (or -1 if unrecognized -> caller should keep '&' literal). */
static long afm_parse_entity(const char *p, const char **p_end)
{
    const char *semi = strchr(p, ';');
    long        cp   = -1;

    if (semi == NULL || (semi - p) > 8) {
        *p_end = p;
        return -1;
    }
    if (*p == '#') {
        const char *q = p + 1;
        unsigned long val = 0UL;
        if (*q == 'x' || *q == 'X') {
            ++q;
            while (q < semi) {
                int d;
                char c = *q++;
                if (c >= '0' && c <= '9') d = c - '0';
                else if (c >= 'a' && c <= 'f') d = 10 + (c - 'a');
                else if (c >= 'A' && c <= 'F') d = 10 + (c - 'A');
                else { *p_end = p; return -1; }
                val = (val << 4) | (unsigned long)d;
            }
        } else {
            while (q < semi) {
                char c = *q++;
                if (c < '0' || c > '9') { *p_end = p; return -1; }
                val = val * 10UL + (unsigned long)(c - '0');
            }
        }
        cp     = (long)val;
        *p_end = semi + 1;
        return cp;
    } else {
        size_t name_len = (size_t)(semi - p);
        size_t i;
        for (i = 0; afm_entities[i].name != NULL; ++i) {
            if (strlen(afm_entities[i].name) == name_len
                && memcmp(p, afm_entities[i].name, name_len) == 0) {
                *p_end = semi + 1;
                return (long)afm_entities[i].cp;
            }
        }
        *p_end = p;
        return -1;
    }
}

/* Compare lowercase: are the next n chars at p equal to lc-string s? */
static int afm_starts_lc(const char *p, const char *s, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) {
        char c = p[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
        if (c != s[i]) return 0;
    }
    return 1;
}

char *afm_html_clean(const char *html_in)
{
    struct afm_strbuf out;
    const char       *p;

    out.data = NULL;
    out.len  = 0;
    out.cap  = 0;
    if (html_in == NULL) {
        if (afm_sb_putc(&out, '\0') != 0) return NULL;
        out.len = 0;
        return out.data;
    }

    p = html_in;
    while (*p != '\0') {
        if (*p == '<') {
            /* <br>, <br/>, <br /> -> newline.
             * <p>, </p> -> newline.
             * Anything else: skip until '>'. */
            const char *q = p + 1;
            int         is_br = 0;
            int         is_p  = 0;
            if (afm_starts_lc(q, "br", 2)) {
                char nx = q[2];
                if (nx == '>' || nx == ' ' || nx == '\t' || nx == '/') is_br = 1;
            }
            if (!is_br) {
                const char *r = q;
                if (*r == '/') ++r;
                if (afm_starts_lc(r, "p", 1)) {
                    char nx = r[1];
                    if (nx == '>' || nx == ' ') is_p = 1;
                }
            }
            while (*q != '\0' && *q != '>') ++q;
            if (*q == '>') ++q;
            p = q;
            if (is_br || is_p) {
                if (afm_sb_putc(&out, '\n') != 0) { free(out.data); return NULL; }
            }
            continue;
        }
        if (*p == '&') {
            const char *end = NULL;
            long        cp  = afm_parse_entity(p + 1, &end);
            if (cp >= 0 && end != p + 1) {
                char buf[4];
                int  n = afm_utf8_encode((unsigned long)cp, buf);
                if (afm_sb_putn(&out, buf, (size_t)n) != 0) { free(out.data); return NULL; }
                p = end;
                continue;
            }
            /* Unknown: keep '&' as literal */
            if (afm_sb_putc(&out, '&') != 0) { free(out.data); return NULL; }
            ++p;
            continue;
        }
        if (afm_sb_putc(&out, *p) != 0) { free(out.data); return NULL; }
        ++p;
    }

    if (out.data == NULL) {
        /* Empty input — return empty string. */
        out.data = (char *)malloc(1);
        if (out.data == NULL) return NULL;
        out.data[0] = '\0';
    }
    return out.data;
}
