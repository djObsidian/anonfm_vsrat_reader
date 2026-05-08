#include "http.h"
#include "parse.h"
#include "colors.h"
#include "render.h"
#include "seen.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define AFM_ANSWERS_URL "https://anon.fm/answers.js"

struct afm_opts {
    int         watch;
    unsigned    interval_sec;
    int         limit;       /* -1 = no limit */
    const char *dj_filter;   /* NULL = no filter */
    const char *colors_path;
    int         no_color;
    const char *from_file;   /* read JSON from a local file instead of HTTP */
};

static void afm_usage(const char *argv0)
{
    fprintf(stderr,
        "anonfm_vsrat_reader — CLI-кукарекалка для anon.fm\n"
        "Usage: %s [options]\n"
        "  -w, --watch              poll mode\n"
        "  -i, --interval SECONDS   poll interval (default 5)\n"
        "  -n, --limit N            show only last N entries\n"
        "  -d, --dj NAME            filter by DJ nick (substring, case-insensitive)\n"
        "  -c, --colors PATH        colour-config file (key/value)\n"
        "      --no-color           disable ANSI colour output\n"
        "      --from-file PATH     read JSON from PATH instead of fetching\n"
        "  -h, --help               show this help\n",
        argv0);
}

static int afm_str_eq(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

/* Case-insensitive substring match (ASCII). For DJ filter. */
static int afm_str_contains_ci(const char *hay, const char *needle)
{
    size_t hl, nl, i;
    if (hay == NULL || needle == NULL) return 0;
    hl = strlen(hay);
    nl = strlen(needle);
    if (nl == 0) return 1;
    if (nl > hl) return 0;
    for (i = 0; i + nl <= hl; ++i) {
        size_t k;
        int    ok = 1;
        for (k = 0; k < nl; ++k) {
            char a = hay[i + k];
            char b = needle[k];
            if (a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = (char)(b + ('a' - 'A'));
            if (a != b) { ok = 0; break; }
        }
        if (ok) return 1;
    }
    return 0;
}

static int afm_parse_args(int argc, char **argv, struct afm_opts *o)
{
    int i;
    o->watch        = 0;
    o->interval_sec = 5;
    o->limit        = -1;
    o->dj_filter    = NULL;
    o->colors_path  = NULL;
    o->no_color     = 0;
    o->from_file    = NULL;

    for (i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (afm_str_eq(a, "-h") || afm_str_eq(a, "--help")) {
            afm_usage(argv[0]);
            return 1;
        } else if (afm_str_eq(a, "-w") || afm_str_eq(a, "--watch")) {
            o->watch = 1;
        } else if (afm_str_eq(a, "--no-color")) {
            o->no_color = 1;
        } else if ((afm_str_eq(a, "-i") || afm_str_eq(a, "--interval")) && i + 1 < argc) {
            int v = atoi(argv[++i]);
            if (v < 1) v = 1;
            o->interval_sec = (unsigned)v;
        } else if ((afm_str_eq(a, "-n") || afm_str_eq(a, "--limit")) && i + 1 < argc) {
            o->limit = atoi(argv[++i]);
        } else if ((afm_str_eq(a, "-d") || afm_str_eq(a, "--dj")) && i + 1 < argc) {
            o->dj_filter = argv[++i];
        } else if ((afm_str_eq(a, "-c") || afm_str_eq(a, "--colors")) && i + 1 < argc) {
            o->colors_path = argv[++i];
        } else if (afm_str_eq(a, "--from-file") && i + 1 < argc) {
            o->from_file = argv[++i];
        } else {
            fprintf(stderr, "unknown option: %s\n", a);
            afm_usage(argv[0]);
            return -1;
        }
    }
    return 0;
}

/* Build a stable dedup key for an entry. Caller-provided buffer must be
 * large enough; we cap each component to keep it sane. Returns the length
 * actually written (without NUL). */
static size_t afm_entry_key(const struct afm_entry *e, char *buf, size_t cap)
{
    size_t      pos = 0;
    const char *parts[4];
    size_t      i;
    char        head_resp[64];
    size_t      hr_len;

    parts[0] = e->listener != NULL ? e->listener : "";
    parts[1] = e->ts       != NULL ? e->ts       : "";
    parts[2] = e->dj       != NULL ? e->dj       : "";
    /* First 32 chars of dj_resp to discriminate edits. */
    if (e->dj_resp != NULL) {
        hr_len = strlen(e->dj_resp);
        if (hr_len > sizeof(head_resp) - 1) hr_len = sizeof(head_resp) - 1;
        memcpy(head_resp, e->dj_resp, hr_len);
        head_resp[hr_len] = '\0';
    } else {
        head_resp[0] = '\0';
    }
    parts[3] = head_resp;

    for (i = 0; i < 4; ++i) {
        size_t n = strlen(parts[i]);
        if (pos + n + 1 >= cap) n = cap - pos - 1;
        memcpy(buf + pos, parts[i], n);
        pos += n;
        if (pos < cap) buf[pos++] = '\x1f';  /* unit-separator */
    }
    if (pos < cap) buf[pos] = '\0';
    else            buf[cap - 1] = '\0';
    return pos;
}

/* Apply --dj filter and --limit, then print in chronological order
 * (oldest at the top, newest at the bottom — same as keke.py). */
static void afm_render_batch(const struct afm_entry *entries, size_t count,
                             const struct afm_opts *o,
                             struct afm_seen *seen, int print_separators)
{
    /* Filter into a small array of indices first. */
    size_t *kept = (size_t *)malloc(sizeof(size_t) * (count == 0 ? 1 : count));
    size_t  kept_n = 0;
    size_t  i;
    int     started = 0;

    if (kept == NULL) return;

    for (i = 0; i < count; ++i) {
        if (o->dj_filter != NULL
            && !afm_str_contains_ci(entries[i].dj, o->dj_filter)) {
            continue;
        }
        kept[kept_n++] = i;
    }

    /* Apply --limit: keep last N (which are the FIRST N in the response —
     * server returns newest-first). */
    if (o->limit >= 0 && (size_t)o->limit < kept_n) {
        kept_n = (size_t)o->limit;
    }

    /* Print in reverse so the newest end up at the bottom. */
    for (i = kept_n; i > 0; --i) {
        size_t idx = kept[i - 1];
        if (seen != NULL) {
            char   key[1024];
            size_t klen = afm_entry_key(&entries[idx], key, sizeof(key));
            int    rc   = afm_seen_check_and_add(seen, key, klen);
            if (rc == 1) continue;  /* already shown */
        }
        afm_render_entry(&entries[idx], print_separators && started);
        started = 1;
    }
    free(kept);
}

static int afm_read_file(const char *path, char **out, size_t *out_len)
{
    FILE  *f;
    long   sz;
    char  *buf;
    size_t got;

    if (out != NULL)     *out     = NULL;
    if (out_len != NULL) *out_len = 0;
    f = fopen(path, "rb");
    if (f == NULL) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }
    buf = (char *)malloc((size_t)sz + 1);
    if (buf == NULL) { fclose(f); return -1; }
    got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { free(buf); return -1; }
    buf[sz] = '\0';
    if (out != NULL)     *out     = buf;
    if (out_len != NULL) *out_len = (size_t)sz;
    return 0;
}

static int afm_fetch_and_render(const struct afm_opts *o,
                                struct afm_seen *seen,
                                int print_separators)
{
    char             *body    = NULL;
    size_t            body_len= 0;
    struct afm_entry *entries = NULL;
    size_t            count   = 0;

    if (o->from_file != NULL) {
        if (afm_read_file(o->from_file, &body, &body_len) != 0) {
            fprintf(stderr, "cannot read %s\n", o->from_file);
            return -1;
        }
    } else {
        if (afm_http_get(AFM_ANSWERS_URL, &body, &body_len) != 0) return -1;
    }
    if (afm_parse_answers(body, body_len, &entries, &count) != 0) {
        free(body);
        return -1;
    }
    afm_render_batch(entries, count, o, seen, print_separators);
    afm_entries_free(entries, count);
    free(body);
    fflush(stdout);
    return 0;
}

int main(int argc, char **argv)
{
    struct afm_opts  opts;
    int              rc;
    struct afm_seen *seen = NULL;

    rc = afm_parse_args(argc, argv, &opts);
    if (rc == 1) return 0;
    if (rc < 0)  return 2;

    afm_console_init();
    if (opts.no_color) afm_colors_disable();
    afm_colors_init(opts.colors_path);

    if (afm_http_global_init() != 0) {
        fprintf(stderr, "curl_global_init failed\n");
        return 1;
    }

    if (opts.watch) {
        seen = afm_seen_new();
        if (seen == NULL) {
            fprintf(stderr, "out of memory\n");
            afm_http_global_cleanup();
            return 1;
        }
        /* Initial fetch: print everything we have, also seed the seen-set. */
        afm_fetch_and_render(&opts, seen, 1);
        for (;;) {
            afm_sleep_ms(opts.interval_sec * 1000U);
            afm_fetch_and_render(&opts, seen, 1);
        }
        /* Unreachable — Ctrl-C exits. */
    } else {
        afm_fetch_and_render(&opts, NULL, 1);
    }

    if (seen != NULL) afm_seen_free(seen);
    afm_http_global_cleanup();
    return 0;
}
