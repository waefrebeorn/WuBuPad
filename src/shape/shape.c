/* shape.c -- HarfBuzz + FriBidi shaping (see shape.h).
 *
 * Implementation is compiled only when HarfBuzz and FriBidi are available
 * (WUBUPAD_WITH_SHAPE). When they are not, the file reduces to tiny stubs so
 * the headless/CI build stays green and callers fall back gracefully. */
#include "shape.h"

#include <stdlib.h>
#include <string.h>

#ifdef WUBUPAD_WITH_SHAPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <fribidi.h>

struct ShapeCtx {
    hb_font_t  *font;      /* HarfBuzz font wrapping the FT face */
    hb_buffer_t *buf;      /* reusable shaping buffer */
};

ShapeCtx *shape_create(void *ft_face) {
    if (!ft_face) return NULL;
    FT_Face face = (FT_Face)ft_face;
    ShapeCtx *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->font = hb_ft_font_create(face, NULL);
    if (!c->font) { free(c); return NULL; }
    c->buf = hb_buffer_create();
    if (!c->buf) { hb_font_destroy(c->font); free(c); return NULL; }
    return c;
}

void shape_destroy(ShapeCtx *ctx) {
    if (!ctx) return;
    if (ctx->buf) hb_buffer_destroy(ctx->buf);
    if (ctx->font) hb_font_destroy(ctx->font);
    free(ctx);
}

/* Convert UTF-8 line to UTF-32 (allocates). Returns count, sets *ucs. */
static int utf8_to_ucs32(const char *s, uint32_t **ucs) {
    size_t n = strlen(s);
    uint32_t *u = malloc((n + 1) * sizeof *u);
    if (!u) { *ucs = NULL; return 0; }
    size_t k = 0;
    for (size_t i = 0; i < n; ) {
        uint32_t cp = 0; unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { cp = c; i += 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; cp = (cp << 6) | (s[i+1] & 0x3F); i += 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; cp = (cp << 6) | (s[i+1] & 0x3F); cp = (cp << 6) | (s[i+2] & 0x3F); i += 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; cp = (cp << 6) | (s[i+1] & 0x3F); cp = (cp << 6) | (s[i+2] & 0x3F); cp = (cp << 6) | (s[i+3] & 0x3F); i += 4; }
        else { cp = c; i += 1; } /* invalid byte: keep raw */
        u[k++] = cp;
    }
    u[k] = 0;
    *ucs = u;
    return (int)k;
}

int shape_line(ShapeCtx *ctx, const char *utf8, int dir,
               ShapeGlyph **out, int *count, int *advance,
               int *caret_x, int caret_cap) {
    if (!ctx || !utf8) return 0;
    uint32_t *ucs = NULL;
    int n = utf8_to_ucs32(utf8, &ucs);
    if (n == 0) { free(ucs); if (count) *count = 0; if (advance) *advance = 0; return 0; }

    /* --- BIDI: determine paragraph direction + visual reorder --- */
    FriBidiParType base = FRIBIDI_PAR_ON; /* auto */
    if (dir == SHAPE_DIR_LTR) base = FRIBIDI_PAR_LTR;
    else if (dir == SHAPE_DIR_RTL) base = FRIBIDI_PAR_RTL;
    /* mirror + shape the RTL runs */
   FriBidiCharType *types = malloc((size_t)n * sizeof *types);
    FriBidiLevel   *levels = malloc((size_t)n * sizeof *levels);
    FriBidiStrIndex *order = malloc((size_t)n * sizeof *order);
    if (!types || !levels || !order) {
        free(types); free(levels); free(order); free(ucs);
        if (count) *count = 0;
        return 0;
    }
    fribidi_get_bidi_types(ucs, n, types);
    FriBidiLevel maxlvl = 0;
    FriBidiLevel emb = fribidi_get_par_embedding_levels(types, n, &base, levels);
    (void)emb;
    for (int i = 0; i < n; i++) if (levels[i] > maxlvl) maxlvl = levels[i];
    /* logical -> visual mapping */
    for (int i = 0; i < n; i++) order[i] = i;
    /* stable reorder by level (FriBidi provides fribidi_reorder_line,
       but we need the visual order array; compute via comparing levels) */
    /* simple bubble-ish: build visual order per FriBidi rules */
    {
        int *vis = malloc((size_t)n * sizeof *vis);
        int vi = 0;
        /* lowest level first; within a level, RTL levels go right-to-left */
        for (int lvl = 0; lvl <= (int)maxlvl; lvl++) {
            if (lvl & 1) { /* RTL: iterate right to left */
                for (int i = n - 1; i >= 0; i--)
                    if ((int)levels[i] == lvl) vis[vi++] = i;
            } else {
                for (int i = 0; i < n; i++)
                    if ((int)levels[i] == lvl) vis[vi++] = i;
            }
        }
        /* vis[] now holds source indices in visual order */
        /* replace order with vis */
        memcpy(order, vis, (size_t)n * sizeof *order);
        free(vis);
    }

    /* --- Shape each visual run sharing a level --- */
    hb_buffer_t *b = ctx->buf;
    /* we accumulate glyphs; cap generously */
    int cap = n * 2 + 8;
    ShapeGlyph *gly = out ? malloc((size_t)cap * sizeof *gly) : NULL;
    int gc = 0;
    int pen_x = 0;
    if (caret_x && caret_cap > 0) for (int i = 0; i < caret_cap; i++) caret_x[i] = 0;

    int i = 0;
    while (i < n) {
        int lvl = levels[order[i]];
        int j = i;
        while (j < n && levels[order[j]] == lvl) j++;
        /* visual run = source indices order[i..j-1] */
        hb_buffer_reset(b);
        for (int k = i; k < j; k++)
            hb_buffer_add_codepoints(b, ucs + order[k], 1, 0, 1);
        hb_buffer_guess_segment_properties(b); /* script + dir + lang */
        /* force direction by run level parity for safety */
        hb_buffer_set_direction(b, (lvl & 1) ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        hb_shape(ctx->font, b, NULL, 0);
        unsigned int ng;
        hb_glyph_info_t *info = hb_buffer_get_glyph_infos(b, &ng);
        hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(b, &ng);
        for (unsigned int g = 0; g < ng; g++) {
            if (gly && gc < cap) {
                gly[gc].glyph = info[g].codepoint;
                gly[gc].x = pen_x + (pos[g].x_offset >> 6);
                gly[gc].y = (pos[g].y_offset >> 6);
                gly[gc].ax = (pos[g].x_advance >> 6);
            }
            if (gc < cap) gc++;
            pen_x += (pos[g].x_advance >> 6);
        }
        i = j;
    }

    /* caret map: for each source char, its pixel x = pen position at its
       cluster. Walk the visual runs again and stamp caret_x[source index]. */
     if (caret_x && caret_cap > 0) {
         int px = 0;
         int ii = 0;
         while (ii < n) {
             int lvl = levels[order[ii]];
             int jj = ii;
             while (jj < n && levels[order[jj]] == lvl) jj++;
             hb_buffer_reset(b);
             for (int k = ii; k < jj; k++)
                 hb_buffer_add_codepoints(b, ucs + order[k], 1, 0, 1);
             hb_buffer_guess_segment_properties(b);
             hb_buffer_set_direction(b, (lvl & 1) ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
             hb_shape(ctx->font, b, NULL, 0);
             unsigned int ng2;
             hb_glyph_info_t *inf2 = hb_buffer_get_glyph_infos(b, &ng2);
             hb_glyph_position_t *po2 = hb_buffer_get_glyph_positions(b, &ng2);
             for (unsigned int g = 0; g < ng2; g++) {
                 unsigned int cl = inf2[g].cluster; /* source index */
                 if ((int)cl < caret_cap) caret_x[cl] = px;
                 px += (po2[g].x_advance >> 6);
             }
             ii = jj;
         }
     }

    free(types); free(levels); free(order); free(ucs);
    if (count) *count = gc;
    if (advance) *advance = pen_x;
    if (out) *out = gly;
    return gc;
}

#else /* !WUBUPAD_WITH_SHAPE -- stubs */

ShapeCtx *shape_create(void *ft_face) { (void)ft_face; return NULL; }
void shape_destroy(ShapeCtx *ctx) { (void)ctx; }
int shape_line(ShapeCtx *ctx, const char *utf8, int dir,
               ShapeGlyph **out, int *count, int *advance,
               int *caret_x, int caret_cap) {
    (void)ctx; (void)utf8; (void)dir; (void)out; (void)caret_x; (void)caret_cap;
    if (count) *count = 0; if (advance) *advance = 0; return 0;
}

#endif /* WUBUPAD_WITH_SHAPE */
