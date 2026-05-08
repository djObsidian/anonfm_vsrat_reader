#include "parse.h"
#include "html.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* jsmn config: parent-link is convenient and we want to be permissive. */
#define JSMN_PARENT_LINKS
#define JSMN_STATIC
#include "jsmn.h"

/* JSON-string unescape into a freshly malloc'd UTF-8 NUL-terminated buffer. */
static char *afm_json_unesc(const char *s, size_t n)
{
    char  *out = (char *)malloc(n + 1);
    size_t i   = 0;
    size_t j   = 0;
    if (out == NULL) return NULL;
    while (i < n) {
        char c = s[i];
        if (c == '\\' && i + 1 < n) {
            char esc = s[i + 1];
            switch (esc) {
                case '"':  out[j++] = '"';  i += 2; break;
                case '\\': out[j++] = '\\'; i += 2; break;
                case '/':  out[j++] = '/';  i += 2; break;
                case 'b':  out[j++] = '\b'; i += 2; break;
                case 'f':  out[j++] = '\f'; i += 2; break;
                case 'n':  out[j++] = '\n'; i += 2; break;
                case 'r':  out[j++] = '\r'; i += 2; break;
                case 't':  out[j++] = '\t'; i += 2; break;
                case 'u': {
                    /* \uXXXX: parse 4 hex digits, encode as UTF-8 */
                    unsigned long cp = 0UL;
                    int           k;
                    if (i + 6 > n) { out[j++] = c; ++i; break; }
                    for (k = 0; k < 4; ++k) {
                        char hc = s[i + 2 + k];
                        int  d;
                        if (hc >= '0' && hc <= '9') d = hc - '0';
                        else if (hc >= 'a' && hc <= 'f') d = 10 + (hc - 'a');
                        else if (hc >= 'A' && hc <= 'F') d = 10 + (hc - 'A');
                        else { d = 0; }
                        cp = (cp << 4) | (unsigned long)d;
                    }
                    if (cp < 0x80UL) {
                        out[j++] = (char)cp;
                    } else if (cp < 0x800UL) {
                        out[j++] = (char)(0xC0U | (cp >> 6));
                        out[j++] = (char)(0x80U | (cp & 0x3FU));
                    } else {
                        out[j++] = (char)(0xE0U | (cp >> 12));
                        out[j++] = (char)(0x80U | ((cp >> 6) & 0x3FU));
                        out[j++] = (char)(0x80U | (cp & 0x3FU));
                    }
                    i += 6;
                    break;
                }
                default:
                    out[j++] = esc;
                    i += 2;
                    break;
            }
        } else {
            out[j++] = c;
            ++i;
        }
    }
    out[j] = '\0';
    return out;
}

/* Extract HH:MM:SS.MMMM from a possibly-HTML-wrapped timestamp like
 *   <span class="timestamp">HH:MM:SS.MMMM</span>
 * Returns malloc'd string. */
static char *afm_extract_timestamp(const char *raw)
{
    const char *p   = raw;
    const char *beg = NULL;
    const char *end = NULL;
    size_t      n;
    char       *out;

    if (raw == NULL) {
        out = (char *)malloc(1);
        if (out != NULL) out[0] = '\0';
        return out;
    }

    /* Find first digit-digit-':'-digit-digit-':'-digit-digit pattern */
    while (*p != '\0') {
        if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1])
            && p[2] == ':'
            && isdigit((unsigned char)p[3]) && isdigit((unsigned char)p[4])
            && p[5] == ':'
            && isdigit((unsigned char)p[6]) && isdigit((unsigned char)p[7])) {
            beg = p;
            end = p + 8;
            /* Optional ".MMMM..." */
            if (*end == '.') {
                ++end;
                while (isdigit((unsigned char)*end)) ++end;
            }
            break;
        }
        ++p;
    }

    if (beg == NULL) {
        /* fallback: clean whole HTML */
        return afm_html_clean(raw);
    }
    n   = (size_t)(end - beg);
    out = (char *)malloc(n + 1);
    if (out == NULL) return NULL;
    memcpy(out, beg, n);
    out[n] = '\0';
    return out;
}

void afm_entries_free(struct afm_entry *entries, size_t count)
{
    size_t i;
    if (entries == NULL) return;
    for (i = 0; i < count; ++i) {
        free(entries[i].listener);
        free(entries[i].listener_msg);
        free(entries[i].ts);
        free(entries[i].dj);
        free(entries[i].dj_resp);
    }
    free(entries);
}

int afm_parse_answers(const char *buf, size_t len,
                      struct afm_entry **out_entries, size_t *out_count)
{
    jsmn_parser  parser;
    jsmntok_t   *tokens   = NULL;
    int          tok_cap  = 1024;
    int          rc;
    int          n;
    int          i;
    int          top;
    int          arr_count;
    int          tok_idx;
    int          item_idx;
    int          entry_w;
    struct afm_entry *entries = NULL;

    if (out_entries != NULL) *out_entries = NULL;
    if (out_count   != NULL) *out_count   = 0;
    if (buf == NULL || len == 0) return -1;

    /* Grow until JSMN_ERROR_NOMEM is gone. */
    for (;;) {
        tokens = (jsmntok_t *)realloc(tokens, sizeof(jsmntok_t) * (size_t)tok_cap);
        if (tokens == NULL) return -1;
        jsmn_init(&parser);
        rc = jsmn_parse(&parser, buf, len, tokens, (unsigned int)tok_cap);
        if (rc != JSMN_ERROR_NOMEM) break;
        tok_cap *= 2;
        if (tok_cap > 1 << 20) { free(tokens); return -1; }
    }
    if (rc < 0) { free(tokens); return -1; }
    n = rc;
    if (n < 1 || tokens[0].type != JSMN_ARRAY) { free(tokens); return -1; }

    top       = 0;
    arr_count = tokens[top].size;
    if (arr_count <= 0) {
        free(tokens);
        if (out_entries != NULL) *out_entries = NULL;
        if (out_count   != NULL) *out_count   = 0;
        return 0;
    }

    entries = (struct afm_entry *)calloc((size_t)arr_count, sizeof(*entries));
    if (entries == NULL) { free(tokens); return -1; }

    /* Walk tokens linearly. The first token after each child-array start is its
     * first element; we use parent_links to find which inner array we're in. */
    tok_idx = 1;
    entry_w = 0;
    for (i = 0; i < arr_count; ++i) {
        jsmntok_t *inner;
        int        inner_size;
        char      *fields[7];
        int        f;

        if (tok_idx >= n) break;
        inner = &tokens[tok_idx];
        if (inner->type != JSMN_ARRAY) {
            /* skip this whole subtree */
            int parent = tok_idx;
            ++tok_idx;
            while (tok_idx < n && tokens[tok_idx].parent >= parent) ++tok_idx;
            continue;
        }
        inner_size = inner->size;
        ++tok_idx;
        for (f = 0; f < 7; ++f) fields[f] = NULL;

        item_idx = 0;
        while (item_idx < inner_size && tok_idx < n) {
            jsmntok_t *t   = &tokens[tok_idx];
            int        beg = t->start;
            int        end = t->end;
            if (t->type == JSMN_STRING) {
                if (item_idx < 7) {
                    fields[item_idx] = afm_json_unesc(buf + beg,
                                                      (size_t)(end - beg));
                    if (fields[item_idx] == NULL) {
                        for (f = 0; f < 7; ++f) free(fields[f]);
                        afm_entries_free(entries, (size_t)entry_w);
                        free(tokens);
                        return -1;
                    }
                }
                ++tok_idx;
                ++item_idx;
            } else if (t->type == JSMN_PRIMITIVE) {
                if (item_idx < 7) {
                    size_t fl = (size_t)(end - beg);
                    fields[item_idx] = (char *)malloc(fl + 1);
                    if (fields[item_idx] != NULL) {
                        memcpy(fields[item_idx], buf + beg, fl);
                        fields[item_idx][fl] = '\0';
                    }
                }
                ++tok_idx;
                ++item_idx;
            } else {
                /* skip this subtree */
                int parent = tok_idx;
                ++tok_idx;
                while (tok_idx < n && tokens[tok_idx].parent >= parent) ++tok_idx;
                ++item_idx;
            }
        }

        /* Need at least 6 fields. Field 6 (real name) is optional. */
        if (fields[1] != NULL && fields[3] != NULL && fields[4] != NULL && fields[5] != NULL) {
            struct afm_entry *e = &entries[entry_w];
            e->announce     = (fields[1] != NULL && strcmp(fields[1], "!") == 0) ? 1 : 0;
            e->listener     = fields[1]; fields[1] = NULL;
            e->listener_msg = afm_html_clean(fields[2] != NULL ? fields[2] : "");
            e->ts           = afm_extract_timestamp(fields[3]);
            e->dj           = fields[4]; fields[4] = NULL;
            e->dj_resp      = afm_html_clean(fields[5] != NULL ? fields[5] : "");
            ++entry_w;
        }
        for (f = 0; f < 7; ++f) free(fields[f]);
    }

    free(tokens);
    if (out_entries != NULL) *out_entries = entries;
    if (out_count   != NULL) *out_count   = (size_t)entry_w;
    return 0;
}
