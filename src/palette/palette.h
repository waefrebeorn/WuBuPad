/* palette.h -- command palette (Cmd-Shift-P).
 *
 * Lists every registered command (via CommandRegistry), fuzzy-filters by the
 * typed query, and runs the chosen one. Headless-friendly: the palette owns
 * no UI; a frontend feeds it keystrokes (palette_feed) and reads the rendered
 * candidate list (palette_render). This mirrors Atom's palette exactly: it is
 * just a fuzzy consumer of the command registry. Opaque, clean C11. */
#ifndef WUBUPAD_PALETTE_H
#define WUBUPAD_PALETTE_H

#include "command.h"
#include <stddef.h>

typedef struct Palette Palette;

/* Create a palette bound to a registry. */
Palette *palette_create(CommandRegistry *reg);
void palette_free(Palette *p);

/* Open/close. When open, keystrokes go to palette_feed instead of the editor. */
void palette_open(Palette *p);
void palette_close(Palette *p);
int  palette_is_open(const Palette *p);

/* Feed a keystroke: printable ASCII edits the query; '\b' (8) deletes last;
 * '\n' (10) confirms the highlighted command (runs it, arg passed through);
 * '\t' (9) / ArrowDown (key 0x1001) / ArrowUp (0x1002) move the highlight.
 * Returns: 0 = still open, 1 = command run + closed, -1 = cancelled (Esc). */
int palette_feed(Palette *p, char ch, int key, void *arg);

/* Number of candidates currently shown. */
size_t palette_count(const Palette *p);
/* Candidate name at visible index i (0 = highlighted). Caller must not free. */
const char *palette_name_at(const Palette *p, size_t i);

/* Highlighted index (0-based). */
size_t palette_highlight(const Palette *p);

/* Current raw query string. */
const char *palette_query(const Palette *p);

#endif /* WUBUPAD_PALETTE_H */
