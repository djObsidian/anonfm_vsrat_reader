#ifndef AFM_HTTP_H
#define AFM_HTTP_H

#include <stddef.h>

/* Process-global init / cleanup. Wrap main() with these. */
int  afm_http_global_init(void);
void afm_http_global_cleanup(void);

/* Set a SOCKS5 proxy (host:port) used for all subsequent GETs. Pass NULL
 * to clear. The string is copied. With a proxy set, DNS is also resolved
 * by the proxy (CURLPROXY_SOCKS5_HOSTNAME). When unset, libcurl still
 * honours ALL_PROXY / http_proxy / https_proxy from the environment. */
void afm_http_set_socks5(const char *host_port);

/* HTTP GET into a heap-allocated buffer.
 * On success returns 0 and sets *out_buf (caller frees) and *out_len.
 * On failure returns non-zero; *out_buf is set to NULL. */
int afm_http_get(const char *url, char **out_buf, size_t *out_len);

#endif
