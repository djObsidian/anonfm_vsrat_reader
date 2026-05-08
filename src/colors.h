#ifndef AFM_COLORS_H
#define AFM_COLORS_H

/* Initialize the colour scheme.
 *  - explicit_path != NULL: try just that file.
 *  - explicit_path == NULL: probe a list of default locations
 *      (./anonfm_colors.conf, $XDG_CONFIG_HOME/anonfm/colors.conf,
 *       $HOME/.config/anonfm/colors.conf, %APPDATA%\anonfm\colors.conf).
 * Always succeeds — falls back to deterministic FNV-1a hash palette. */
void afm_colors_init(const char *explicit_path);

/* Returns an xterm-256 colour index (0..255) for the given listener / DJ
 * nick. Returns -1 if the global "no colour" override is on. */
int  afm_color_for_listener(const char *nick);
int  afm_color_for_dj      (const char *nick);

/* Disable colour entirely. After this call both functions return -1. */
void afm_colors_disable(void);

#endif
