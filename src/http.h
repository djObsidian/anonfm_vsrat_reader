#ifndef AFM_HTTP_H
#define AFM_HTTP_H

#include <stddef.h>

/* Process-global init / cleanup. Wrap main() with these. */
int  afm_http_global_init(void);
void afm_http_global_cleanup(void);

/* HTTP GET into a heap-allocated buffer.
 * On success returns 0 and sets *out_buf (caller frees) and *out_len.
 * On failure returns non-zero; *out_buf is set to NULL. */
int afm_http_get(const char *url, char **out_buf, size_t *out_len);

#endif
