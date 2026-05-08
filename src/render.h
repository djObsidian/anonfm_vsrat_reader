#ifndef AFM_RENDER_H
#define AFM_RENDER_H

#include "parse.h"

/* Render one entry to stdout, in screen-friendly layout (similar to
 * reference.png). With_separator controls whether a leading "—" rule
 * line is emitted above the entry. */
void afm_render_entry(const struct afm_entry *e, int with_separator);

#endif
