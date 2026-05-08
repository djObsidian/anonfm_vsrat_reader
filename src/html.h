#ifndef AFM_HTML_H
#define AFM_HTML_H

/* Returns a heap-allocated, NUL-terminated UTF-8 string with HTML tags
 * stripped (and <br> turned into '\n') and HTML entities decoded.
 * Caller frees. Returns NULL on allocation failure. */
char *afm_html_clean(const char *html_in);

#endif
