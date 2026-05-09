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

/* Returns an xterm-256 colour index (0..255) to paint the BODY TEXT of the
 * listener message / DJ response. Returns -1 if the global "no colour"
 * override is on, OR if the config rule for this nick is `off` (the
 * default — bodies are uncoloured unless the config explicitly enables
 * a colour or `hash`). */
int  afm_color_for_listener_text(const char *nick);
int  afm_color_for_dj_text      (const char *nick);

/* Disable colour entirely. After this call all the getters above return -1. */
void afm_colors_disable(void);

/* 1 if colour output is on, 0 if --no-color was passed. */
int  afm_color_globally_enabled(void);

#endif
