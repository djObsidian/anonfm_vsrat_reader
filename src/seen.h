#ifndef AFM_SEEN_H
#define AFM_SEEN_H

#include <stddef.h>

struct afm_seen;

struct afm_seen *afm_seen_new(void);
void             afm_seen_free(struct afm_seen *s);

/* Returns 1 if the key was already present, 0 if it was just inserted.
 * On allocation failure returns -1. */
int afm_seen_check_and_add(struct afm_seen *s, const char *key, size_t key_len);

#endif
