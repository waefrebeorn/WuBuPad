/* shape.h -- text shaping + BIDI for the graphics backend.
 *
 * Wraps HarfBuzz (hb-ft) for OpenType shaping and FriBidi for Unicode
 * bidirectional reordering, behind a clean C11 ABI. This is what unblocks
 * correct rendering of CJK, Arabic/Hebrew (RTL), Indic, Thai and all complex
 * scripts -- FreeType alone cannot do this.
 *
 * Optional: if WuBuPad is built without HarfBuzz/FriBidi, shape_create()
 * returns NULL and the caller keeps its legacy per-codepoint path.
 *
 * Clean C11. No globals. Caller owns the returned glyph array (free() it). */
#ifndef WUBUPAD_SHAPE_H
#define WUBUPAD_SHAPE_H

#include <stddef.h>

typedef struct ShapeCtx ShapeCtx;

/* A positioned glyph in visual order. x/ax are in 26.6? No -- already pixel
 * integers from the FreeType scale we set. y is the vertical offset from the
 * line baseline (positive = up in our renderer). */
typedef struct {
    unsigned int glyph;   /* FreeType glyph index (uint32) */
    int x;                /* pen x at which to draw (pixels, visual order) */
    int y;                /* vertical offset from baseline (pixels) */
    int ax;               /* x advance (pixels) */
} ShapeGlyph;

/* dir: 0 = auto (use FriBidi paragraph detection), 1 = LTR, 2 = RTL. */
#define SHAPE_DIR_AUTO 0
#define SHAPE_DIR_LTR  1
#define SHAPE_DIR_RTL  2

/* Create a shaping context bound to an already-open FreeType face.
 * Returns NULL if shaping support is unavailable. The face must outlive ctx. */
ShapeCtx *shape_create(void *ft_face);

/* Destroy a shaping context (does NOT free the FT face). */
void shape_destroy(ShapeCtx *ctx);

/* Shape one UTF-8 line.
 *   utf8     : NUL-terminated line text
 *   dir      : base direction hint (SHAPE_DIR_*)
 *   out      : *out is set to a malloc'd ShapeGlyph[] (caller free()s), or
 *              NULL if out==NULL (measure only)
 *   count    : receives the number of glyphs
 *   advance  : receives the total visual width in pixels
 *   caret_x  : optional caller buffer (size caret_cap) receiving the pixel x
 *              of each SOURCE character boundary (for caret placement); the
 *              mapping is by original char index, robust to RTL reordering
 *   caret_cap: capacity of caret_x (0 disables)
 * Returns the glyph count (0 on empty / error). */
int shape_line(ShapeCtx *ctx, const char *utf8, int dir,
               ShapeGlyph **out, int *count, int *advance,
               int *caret_x, int caret_cap);

#endif /* WUBUPAD_SHAPE_H */
