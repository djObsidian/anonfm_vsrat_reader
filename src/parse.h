#ifndef AFM_PARSE_H
#define AFM_PARSE_H

#include <stddef.h>

struct afm_entry {
    int   announce;       /* 1 if listener field is "!" */
    char *listener;       /* nick or "!" */
    char *listener_msg;   /* HTML-decoded plaintext, may be empty */
    char *ts;             /* "HH:MM:SS.MMMM" */
    char *dj;             /* DJ nick */
    char *dj_resp;        /* HTML-decoded plaintext */
};

/* Parse the JSON returned by https://anon.fm/answers.js into an array of
 * entries. *out_entries is heap-allocated, *out_count holds the size.
 * Returns 0 on success. Caller must afm_entries_free(...). */
int  afm_parse_answers(const char *buf, size_t len,
                       struct afm_entry **out_entries, size_t *out_count);

void afm_entries_free(struct afm_entry *entries, size_t count);

#endif
