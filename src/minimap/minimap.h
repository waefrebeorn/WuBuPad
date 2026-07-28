/* minimap.h -- code minimap (Atom "minimap" package).
 *
 * Builds a downsampled overview of a document: each source line maps to a
 * fixed-height mini-row; non-blank lines are "lit". Headless-friendly: the
 * engine produces a compact bitmap description (per-row lit count) the UI
 * scales onto a side panel. Opaque, C11, no deps. */
#ifndef WUBUPAD_MINIMAP_H
#define WUBUPAD_MINIMAP_H

#include <stddef.h>

typedef struct Minimap Minimap;

Minimap *minimap_create(int rows, int cols);   /* mini-grid dimensions */
void minimap_free(Minimap *m);

/* Recompute the overview from `text` (NUL-terminated). Each source line
 * becomes one minimap row (capped at `rows`); a row is "lit" if it contains
 * non-whitespace. Returns number of source lines mapped. */
size_t minimap_update(Minimap *m, const char *text);

/* Is minimap row `r` lit (has content)? */
int  minimap_lit(const Minimap *m, size_t r);
/* Number of lit rows (for a quick "code density" stat). */
size_t minimap_lit_count(const Minimap *m);
/* Total rows in the minimap. */
size_t minimap_rows(const Minimap *m);

#endif /* WUBUPAD_MINIMAP_H */
