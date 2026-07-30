/* ui_gfx.c -- real graphics backend for the UI layer (SDL2 + FreeType2).
 *
 * Owns only view rendering + input translation. All editing still funnels
 * through Doc via ui.c. Uses a glyph atlas (one cached texture per codepoint)
 * + per-line glyph blits, dirty-region is handled by the driver (it redraws
 * the whole viewport each frame; cheap at editor sizes). Semantic color tokens
 * drive syntax highlighting + theme. No toolkit fork; core never sees SDL/FT.
 *
 * Headless/CI: if SDL_VIDEODRIVER=dummy, init still succeeds (no real window)
 * so tests can exercise draw_line/present without a display. */
#include "ui.h"
#include "ui_gfx.h"
#include "ui_theme.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "shape/shape.h"   /* optional HarfBuzz/FriBidi shaping */

/* Token kinds used by draw_line's `kind` arg (mirrors LexTok from lex.h). */
typedef enum {
    TKN_PLAIN = 0, TKN_KW, TKN_TYPE, TKN_STR, TKN_CHAR,
    TKN_NUM, TKN_COMMENT, TKN_PREPROC
} GfxTok;

#define FONT_PT 18

typedef struct {
    SDL_Texture *tex;     /* per-glyph texture (works on any driver) */
    int w, h;             /* glyph bitmap size */
    int bx, by;           /* glyph bbox offset (bx also stashes codepoint) */
    int ax;               /* advance x */
    int gidx;             /* FreeType glyph index (-1 if cached by codepoint) */
} Glyph;

typedef struct {
    int cols, rows;
    int char_w, line_h;    /* metrics (monospace-ish) */
    SDL_Window *win;
    SDL_Renderer *ren;
    FT_Library ft;
    FT_Face face;
    Glyph *glyphs;
    int nglyphs, capglyph;
    UITheme *theme;        /* semantic palette (persisted) */
    int caret_on;
    int init_ok;
    ShapeCtx *shape;       /* text shaping (NULL = legacy path) */
    int *caret_x;          /* per-row source-char pixel x (row*CARET_STRIDE) */
    int caret_row;         /* cursor view-row (for active-line band) */
} GFX;

/* forward decl: color resolver defined below, used by blit helpers */
static UIRGB gfx_color(GFX *g, UIToken tok);

/* --- glyph cache (per-glyph texture; no render-target needed) --- */
static int gfx_glyph_index(GFX *g, Uint32 cp) {
    /* cp stashed in bx field for O(1) find via linear scan (editor glyph set small) */
    for (int i = 0; i < g->nglyphs; i++)
        if ((Uint32)g->glyphs[i].bx == cp) return i;
    return -1;
}
static int gfx_glyph_index_by_glyph(GFX *g, unsigned int gidx) {
    for (int i = 0; i < g->nglyphs; i++)
        if ((unsigned int)g->glyphs[i].gidx == gidx) return i;
    return -1;
}
static int gfx_cache_glyph(GFX *g, Uint32 cp) {
    if (g->nglyphs >= g->capglyph) {
        int nc = g->capglyph ? g->capglyph * 2 : 256;
        Glyph *ng = realloc(g->glyphs, nc * sizeof *ng);
        if (!ng) return -1;
        g->glyphs = ng; g->capglyph = nc;
    }
    if (FT_Load_Char(g->face, cp ? cp : (Uint32)' ', FT_LOAD_RENDER) != 0) return -1;
    FT_GlyphSlot slot = g->face->glyph;
    int w = slot->bitmap.width, h = slot->bitmap.rows;
    Glyph *gl = &g->glyphs[g->nglyphs];
    memset(gl, 0, sizeof *gl);
    gl->bx = (int)cp;  /* stash codepoint */
    gl->gidx = -1;
    gl->ax = (int)(slot->advance.x >> 6);
    gl->w = w; gl->h = h;
    gl->by = slot->bitmap_top;

    if (w > 0 && h > 0) {
        SDL_Texture *t = SDL_CreateTexture(g->ren, SDL_PIXELFORMAT_RGBA32,
                                           SDL_TEXTUREACCESS_STATIC, w, h);
        if (!t) return -1;
        unsigned char *pix = malloc((size_t)w * h * 4);
        if (!pix) { SDL_DestroyTexture(t); return -1; }
        const unsigned char *src = slot->bitmap.buffer;
        for (int yy = 0; yy < h; yy++) {
            for (int xx = 0; xx < w; xx++) {
                unsigned char a = src[yy * slot->bitmap.pitch + xx];
                pix[(yy * w + xx) * 4 + 0] = 255;
                pix[(yy * w + xx) * 4 + 1] = 255;
                pix[(yy * w + xx) * 4 + 2] = 255;
                pix[(yy * w + xx) * 4 + 3] = a;
            }
        }
        SDL_UpdateTexture(t, NULL, pix, w * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        free(pix);
        gl->tex = t;
    }
    return g->nglyphs++;
}
/* cache by FreeType glyph index (shaping path) */
static int gfx_cache_glyph_index(GFX *g, unsigned int gidx) {
    if (g->nglyphs >= g->capglyph) {
        int nc = g->capglyph ? g->capglyph * 2 : 256;
        Glyph *ng = realloc(g->glyphs, nc * sizeof *ng);
        if (!ng) return -1;
        g->glyphs = ng; g->capglyph = nc;
    }
    if (FT_Load_Glyph(g->face, gidx, FT_LOAD_RENDER) != 0) return -1;
    FT_GlyphSlot slot = g->face->glyph;
    int w = slot->bitmap.width, h = slot->bitmap.rows;
    Glyph *gl = &g->glyphs[g->nglyphs];
    memset(gl, 0, sizeof *gl);
    gl->gidx = (int)gidx;
    gl->bx = -1;
    gl->ax = (int)(slot->advance.x >> 6);
    gl->w = w; gl->h = h;
    gl->by = slot->bitmap_top;
    if (w > 0 && h > 0) {
        SDL_Texture *t = SDL_CreateTexture(g->ren, SDL_PIXELFORMAT_RGBA32,
                                           SDL_TEXTUREACCESS_STATIC, w, h);
        if (!t) return -1;
        unsigned char *pix = malloc((size_t)w * h * 4);
        if (!pix) { SDL_DestroyTexture(t); return -1; }
        const unsigned char *src = slot->bitmap.buffer;
        for (int yy = 0; yy < h; yy++) {
            for (int xx = 0; xx < w; xx++) {
                unsigned char a = src[yy * slot->bitmap.pitch + xx];
                pix[(yy * w + xx) * 4 + 0] = 255;
                pix[(yy * w + xx) * 4 + 1] = 255;
                pix[(yy * w + xx) * 4 + 2] = 255;
                pix[(yy * w + xx) * 4 + 3] = a;
            }
        }
        SDL_UpdateTexture(t, NULL, pix, w * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        free(pix);
        gl->tex = t;
    }
    return g->nglyphs++;
}
/* blit a glyph by index (shaping path) */
static void gfx_blit_glyph(GFX *g, unsigned int gidx, int px, int py, UIToken tok) {
    int gi = gfx_glyph_index_by_glyph(g, gidx);
    if (gi < 0) gi = gfx_cache_glyph_index(g, gidx);
    if (gi < 0) return;
    Glyph *gl = &g->glyphs[gi];
    if (!gl->tex || gl->w <= 0 || gl->h <= 0) return;
    UIRGB c = gfx_color(g, tok);
    SDL_SetTextureColorMod(gl->tex, c.r, c.g, c.b);
    SDL_Rect dst = { px, py - gl->by + g->line_h, gl->w, gl->h };
    SDL_RenderCopy(g->ren, gl->tex, NULL, &dst);
}

static UIRGB gfx_color(GFX *g, UIToken tok) {
    return ui_theme_color(g->theme, tok);
}

/* Returns the glyph advance (px) so callers can advance by real width,
 * NOT by a fixed cell width (critical for correct UTF-8 / proportional text). */
static int gfx_draw_glyph(GFX *g, int cp, int px, int py, UIToken tok) {
    int gi = gfx_glyph_index(g, (Uint32)cp);
    if (gi < 0) gi = gfx_cache_glyph(g, (Uint32)(cp ? cp : ' '));
    if (gi < 0) return g->char_w;
    Glyph *gl = &g->glyphs[gi];
    if (!gl->tex || gl->w <= 0 || gl->h <= 0) return g->char_w;
    UIRGB c = gfx_color(g, tok);
    SDL_SetTextureColorMod(gl->tex, c.r, c.g, c.b);
    SDL_Rect dst = { px + gl->bx, py - gl->by + g->line_h, gl->w, gl->h };
    SDL_RenderCopy(g->ren, gl->tex, NULL, &dst);
    return gl->ax;
}

/* --- vtable --- */
static int gfx_init(void **st, int cols, int rows) {
    GFX *g = calloc(1, sizeof *g);
    if (!g) return -1;
    g->cols = cols > 0 ? cols : 100;
    g->rows = rows > 0 ? rows : 40;
    g->theme = ui_theme_create();
    if (g->theme) ui_theme_load(g->theme);
    g->caret_on = 1;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "gfx_init: SDL_Init failed: %s\n", SDL_GetError());
        free(g); return -1;
    }
    g->win = SDL_CreateWindow("WuBuPad",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              g->cols * 9, g->rows * FONT_PT,
                              SDL_WINDOW_RESIZABLE);
    if (!g->win) {
        fprintf(stderr, "gfx_init: CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit(); free(g); return -1;
    }
    g->ren = SDL_CreateRenderer(g->win, -1, SDL_RENDERER_ACCELERATED);
    if (!g->ren) g->ren = SDL_CreateRenderer(g->win, -1, SDL_RENDERER_SOFTWARE);
    if (!g->ren) {
        fprintf(stderr, "gfx_init: CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g->win); SDL_Quit(); free(g); return -1;
    }

    if (FT_Init_FreeType(&g->ft) != 0) {
        fprintf(stderr, "gfx_init: FT_Init_FreeType failed\n");
        SDL_DestroyRenderer(g->ren); SDL_DestroyWindow(g->win); SDL_Quit(); free(g); return -1;
    }
    /* Prefer a modern humanist mono (Ubuntu Sans Mono) for cleaner glyph
     * shapes + hinting; fall back to DejaVu if absent. (Spec §3: one mono.) */
    const char *font = "/usr/share/fonts/truetype/ubuntu/UbuntuSansMono[wght].ttf";
    if (FT_New_Face(g->ft, font, 0, &g->face) != 0) {
        font = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
        if (FT_New_Face(g->ft, font, 0, &g->face) != 0) {
            font = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
            if (FT_New_Face(g->ft, font, 0, &g->face) != 0) {
                fprintf(stderr, "gfx_init: FT_New_Face failed for %s\n", font);
                FT_Done_FreeType(g->ft); SDL_DestroyRenderer(g->ren);
                SDL_DestroyWindow(g->win); SDL_Quit(); free(g); return -1;
            }
        }
    }
    FT_Set_Pixel_Sizes(g->face, 0, FONT_PT);

#ifdef WUBUPAD_WITH_SHAPE
    g->shape = shape_create(g->face);
    if (!g->shape) fprintf(stderr, "gfx_init: shaping unavailable (HarfBuzz/FriBidi)\n");
    g->caret_x = calloc((size_t)g->rows * 4096, sizeof(int));
#else
    g->shape = NULL;
    g->caret_x = NULL;
#endif

    /* metrics: use ascii 'M' advance + line height */
    int gi = gfx_cache_glyph(g, (Uint32)'M');
    g->char_w = (gi >= 0) ? g->glyphs[gi].ax : 9;
    if (g->char_w <= 0) g->char_w = 9;
    g->line_h = FONT_PT + 4;

    /* recompute viewport from real window size */
    int ww, wh;
    SDL_GetWindowSize(g->win, &ww, &wh);
    g->cols = ww / g->char_w;
    g->rows = wh / g->line_h;

    g->init_ok = 1;
    *st = g;
    return 0;
}

static void gfx_destroy(void *st) {
    GFX *g = st;
    if (!g) return;
    if (g->glyphs) {
        for (int i = 0; i < g->nglyphs; i++)
            if (g->glyphs[i].tex) SDL_DestroyTexture(g->glyphs[i].tex);
        free(g->glyphs);
    }
    if (g->theme) ui_theme_free(g->theme);
    if (g->shape) shape_destroy(g->shape);
    if (g->caret_x) free(g->caret_x);
    if (g->face) FT_Done_Face(g->face);
    if (g->ft) FT_Done_FreeType(g->ft);
    if (g->ren) SDL_DestroyRenderer(g->ren);
    if (g->win) SDL_DestroyWindow(g->win);
    SDL_Quit();
    free(g);
}

static void gfx_fill(GFX *g, int x, int y, int w, int h, UIToken tok) {
    UIRGB c = gfx_color(g, tok);
    SDL_SetRenderDrawColor(g->ren, c.r, c.g, c.b, 255);
    SDL_Rect r = { x, y, w, h };
    SDL_RenderFillRect(g->ren, &r);
}

static int gfx_kind_token(int kind) {
    switch (kind) {
        case 1: return TOK_KW;
        case 2: return TOK_TYPE;
        case 3: return TOK_STR;
        case 4: return TOK_STR;   /* char */
        case 5: return TOK_NUM;
        case 6: return TOK_COMMENT;
        case 7: return TOK_PREPROC;
        default: return TOK_TEXT;
    }
}

static void gfx_draw_line(void *st, int row, const char *text, int len, int kind) {
    GFX *g = st;
    if (!g || !g->init_ok || row < 0 || row >= g->rows) return;
    int py = row * g->line_h;
    /* Active-line band: the cursor's row gets a raised surface so the eye
     * locks onto the working line (VS Code / Sublime pattern, spec §4). */
    int band = (row == g->caret_row);
    gfx_fill(g, 0, py, g->cols * g->char_w, g->line_h,
             band ? TOK_SURFACE_3 : TOK_SURFACE);
    UIToken tok = (UIToken)gfx_kind_token(kind);
    int n = len < 0 ? (int)strlen(text ? text : "") : len;
    int px0 = g->char_w * 5 + 8;   /* gutter inset on the 4px grid (spec §4) */

#ifdef WUBUPAD_WITH_SHAPE
    if (g->shape) {
        ShapeGlyph *gly = NULL;
        int gc = 0, adv = 0;
        int cap = (n > 4096) ? 4096 : n;
        int *cx = (g->caret_x && cap > 0) ? &g->caret_x[(size_t)row * 4096] : NULL;
        shape_line(g->shape, text ? text : "", SHAPE_DIR_AUTO, &gly, &gc, &adv, cx, cap);
        int px = px0;
        for (int i = 0; i < gc; i++) {
            gfx_blit_glyph(g, gly[i].glyph, px + gly[i].x, py + gly[i].y, tok);
            px += gly[i].ax;
        }
        free(gly);
        return;
    }
#endif
    int px = px0;
    /* Decode UTF-8 into codepoints (NOT bytes) and advance by the real glyph
     * advance. Byte-wise rendering is the classic mojibake bug (cafe -> c a f
     * A- M-). This fallback path only runs when shaping is disabled. */
    for (int i = 0; i < n; ) {
        unsigned int cp;
        unsigned char b0 = (unsigned char)text[i];
        if (b0 < 0x80)            { cp = b0;                                   i += 1; }
        else if ((b0 & 0xE0)==0xC0){ cp = (b0&0x1F)<<6;  if (i+1<n) cp |= (unsigned char)text[i+1]&0x3F; i += 2; }
        else if ((b0 & 0xF0)==0xE0){ cp = (b0&0x0F)<<12; if (i+1<n) cp |= ((unsigned char)text[i+1]&0x3F)<<6; if (i+2<n) cp |= (unsigned char)text[i+2]&0x3F; i += 3; }
        else if ((b0 & 0xF8)==0xF0){ cp = (b0&0x07)<<18; if (i+1<n) cp |= ((unsigned char)text[i+1]&0x3F)<<12; if (i+2<n) cp |= ((unsigned char)text[i+2]&0x3F)<<6; if (i+3<n) cp |= (unsigned char)text[i+3]&0x3F; i += 4; }
        else                      { cp = b0;                                   i += 1; } /* invalid lead byte */
        if (cp == 0) break;
        px += gfx_draw_glyph(g, (int)cp, px, py, tok);
    }
}

static void gfx_draw_caret(void *st, int row, int col) {
    GFX *g = st;
    if (!g || !g->init_ok) return;
    g->caret_row = row;   /* remember for the active-line band */
    int x;
#ifdef WUBUPAD_WITH_SHAPE
    if (g->shape && g->caret_x && row >= 0 && row < g->rows) {
        int idx = col; if (idx < 0) idx = 0; if (idx > 4095) idx = 4095;
        x = g->char_w * 5 + 2 + g->caret_x[(size_t)row * 4096 + idx];
    } else {
        x = g->char_w * 5 + 2 + col * g->char_w;
    }
#else
    x = g->char_w * 5 + 2 + col * g->char_w;
#endif
    int y = row * g->line_h;
    gfx_fill(g, x, y, 2, g->line_h, TOK_CARET);
}

/* --- chrome (Phase C) --- */
static int gfx_chrome_rows(void *st) {
    (void)st;
    return 2;   /* 1 tab strip + 1 status bar */
}

static void gfx_draw_gutter(void *st, int row, int line_no) {
    GFX *g = st;
    if (!g || !g->init_ok || row < 0 || row >= g->rows) return;
    if (line_no <= 0) return;   /* past end of buffer */
    int py = row * g->line_h;
    /* Gutter sits on SURFACE_2; the active line's gutter rises to SURFACE_3
     * so the band reads as one continuous strip. */
    int band = (row == g->caret_row);
    gfx_fill(g, 0, py, g->char_w * 5, g->line_h,
             band ? TOK_SURFACE_3 : TOK_SURFACE_2);
    /* 1px divider between gutter and text (spec §1 border role). */
    gfx_fill(g, g->char_w * 5 - 1, py, 1, g->line_h, TOK_BORDER);
    char buf[16];
    int n = snprintf(buf, sizeof buf, "%d", line_no);
    int px = 8;   /* 4px-grid inset */
    for (int i = 0; i < n && i < 5; i++)
        gfx_draw_glyph(g, (unsigned char)buf[i], px + i * g->char_w, py,
                       band ? TOK_TEXT_DIM : TOK_TEXT_DIM);
}

static void gfx_draw_tab(void *st, int tab_row, int index,
                         const char *name, int active, int dirty) {
    GFX *g = st;
    if (!g || !g->init_ok || tab_row != 0) return;
    /* Tab = a neutral segment on SURFACE_2; the ACTIVE tab rises to SURFACE_3
     * and gets a 2px accent underline. No bright blue fill (spec §5: one
     * accent used for emphasis only). Inactive tabs use dim text. */
    int tw = g->char_w * 22;
    int w = tw;
    gfx_fill(g, index * tw, 0, w, g->line_h, active ? TOK_SURFACE_3 : TOK_SURFACE_2);
    /* 1px divider to the right of each tab. */
    gfx_fill(g, index * tw + w - 1, 0, 1, g->line_h, TOK_BORDER);
    if (active)
        gfx_fill(g, index * tw, g->line_h - 2, w, 2, TOK_ACCENT);  /* accent underline */

    /* label: dirty shows an accent dot (not just '*') + name; active text is
     * primary, inactive is dim. Don't rely on color alone (spec §1/§5). */
    int tx = index * tw + 12;
    int ty = (g->line_h - g->char_w) / 2;   /* center mono glyph in tab */
    if (dirty) {
        gfx_fill(g, tx, ty + 2, 6, 6, TOK_ACCENT);   /* dirty dot */
        tx += 12;
    }
    char buf[32];
    int n = snprintf(buf, sizeof buf, "%s%s", name ? name : "",
                     dirty ? "" : "");
    for (int i = 0; i < n && tx < index * tw + w - 8; i++)
        gfx_draw_glyph(g, (unsigned char)buf[i], tx + i * g->char_w, 0,
                       active ? TOK_TEXT : TOK_TEXT_DIM);
}

static void gfx_draw_status(void *st, int row, const char *text) {
    GFX *g = st;
    if (!g || !g->init_ok || row != g->rows - 1) return;
    int py = row * g->line_h;
    /* Quiet chrome band on SURFACE_2, 1px top border, dim text (spec §6). */
    gfx_fill(g, 0, py, g->cols * g->char_w, g->line_h, TOK_SURFACE_2);
    gfx_fill(g, 0, py, g->cols * g->char_w, 1, TOK_BORDER);
    int n = (int)strlen(text ? text : "");
    int px = 12;   /* 4px-grid inset */
    for (int i = 0; i < n; i++)
        gfx_draw_glyph(g, (unsigned char)text[i], px + i * g->char_w, py, TOK_TEXT_DIM);
}

static void gfx_set_theme(void *st, int dark) {
    GFX *g = st;
    if (!g || !g->theme) return;
    ui_theme_set_dark(g->theme, dark);
}

/* function-list panel: draw symbol name right-aligned in the right gutter. */
static void gfx_draw_symbols(void *st, int row, const char *name, int line_no) {
    GFX *g = st;
    if (!g || !g->init_ok) return;
    int panel_w = 24 * g->char_w;           /* panel width */
    int x0 = g->cols * g->char_w - panel_w;
    int py = row * g->line_h;
    gfx_fill(g, x0, py, panel_w, g->line_h, TOK_SURFACE_2);
    gfx_fill(g, x0, py, 1, g->line_h, TOK_BORDER);
    char buf[40];
    snprintf(buf, sizeof buf, "%s : L%d", name ? name : "", line_no + 1);
    int n = (int)strlen(buf);
    int px = x0 + 8;
    for (int i = 0; i < n; i++)
        gfx_draw_glyph(g, (unsigned char)buf[i], px + i * g->char_w, py, TOK_TEXT_DIM);
}

static void gfx_present(void *st) {
    GFX *g = st;
    if (!g || !g->init_ok) return;
    SDL_RenderPresent(g->ren);
}

static int gfx_get_key(void *st, char *ch, int *key) {
    GFX *g = st;
    (void)g;
    *ch = 0; *key = UI_KEY_NONE;
    SDL_Event e;
    if (SDL_WaitEvent(&e) == 0) return -1;
    if (e.type == SDL_QUIT) { *key = UI_KEY_QUIT; return -1; }
    if (e.type == SDL_WINDOWEVENT &&
        e.window.event == SDL_WINDOWEVENT_RESIZED) {
        /* handled by resize(); treat as no-op key */
        return 0;
    }
    if (e.type != SDL_KEYDOWN) return 0;
    SDL_Keycode k = e.key.keysym.sym;
    SDL_Keymod mod = e.key.keysym.mod;
    /* ctrl combos */
    if (mod & KMOD_CTRL) {
        switch (k) {
            case SDLK_q: *key = UI_KEY_QUIT; return -1;
            case SDLK_z: *key = UI_KEY_UNDO; return 0;
            case SDLK_y: *key = UI_KEY_REDO; return 0;
            case SDLK_f: *key = (mod & KMOD_SHIFT) ? UI_KEY_FOLD : UI_KEY_FIND; return 0;
            case SDLK_l: *key = (mod & KMOD_SHIFT) ? UI_KEY_SYMBOLS : UI_KEY_NONE; return 0;
            case SDLK_a: *key = UI_KEY_SELECT_ALL; return 0;
            case SDLK_x: *key = UI_KEY_CUT; return 0;
            case SDLK_v: *key = (mod & KMOD_SHIFT) ? UI_KEY_PASTE_PLAIN : UI_KEY_PASTE; return 0;
            case SDLK_c: *key = (mod & KMOD_SHIFT) ? UI_KEY_COLMODE : UI_KEY_COPY; return 0;
            case SDLK_e: *key = (mod & KMOD_SHIFT) ? UI_KEY_EOL : UI_KEY_END; return 0;
            case SDLK_r: *key = (mod & KMOD_SHIFT) ? UI_KEY_MACRO : UI_KEY_NONE; return 0;
            case SDLK_p: *key = (mod & KMOD_SHIFT) ? UI_KEY_REPLAY : UI_KEY_NONE; return 0;
            case SDLK_SPACE:
                if (mod & KMOD_CTRL) { *key = UI_KEY_COMPLETE; return 0; }
                break;
            case SDLK_LEFT: case SDLK_b: *key = UI_KEY_LEFT; return 0;
            case SDLK_RIGHT: *key = UI_KEY_RIGHT; return 0;
        }
    }
    switch (k) {
        case SDLK_LEFT:  *key = UI_KEY_LEFT;  break;
        case SDLK_RIGHT: *key = UI_KEY_RIGHT; break;
        case SDLK_UP:    *key = UI_KEY_UP;    break;
        case SDLK_DOWN:  *key = UI_KEY_DOWN;  break;
        case SDLK_HOME:  *key = UI_KEY_HOME;  break;
        case SDLK_END:   *key = UI_KEY_END;   break;
        case SDLK_PAGEUP:   *key = UI_KEY_PGUP;   break;
        case SDLK_PAGEDOWN: *key = UI_KEY_PGDOWN; break;
        case SDLK_BACKSPACE: *key = UI_KEY_BACKSPACE; break;
        case SDLK_DELETE:    *key = UI_KEY_DEL; break;
        case SDLK_RETURN: case SDLK_KP_ENTER: *key = UI_KEY_ENTER; break;
        case SDLK_ESCAPE: *key = UI_KEY_QUIT; return -1;
        case SDLK_F5: *key = UI_KEY_THEME; break;
        default:
            if (k >= 32 && k < 127) { *ch = (char)k; *key = UI_KEY_NONE; }
            else { *key = UI_KEY_NONE; }
            break;
    }
    return 0;
}

static void gfx_resize(void *st, int *cols, int *rows) {
    GFX *g = st;
    if (!g || !g->init_ok) return;
    int ww, wh;
    SDL_GetWindowSize(g->win, &ww, &wh);
    g->cols = ww / g->char_w;
    g->rows = wh / g->line_h;
    *cols = g->cols; *rows = g->rows;
}

/* vtable capture shim: maps the vtable signature onto ui_gfx_capture.
 * Defined before ui_gfx_backend() so the vtable can reference it. */
static int gfx_capture_shim(void *st, unsigned char **rgba, int *w, int *h) {
    return ui_gfx_capture(st, rgba, w, h);
}

const UI_Backend *ui_gfx_backend(void) {
    static const UI_Backend b = {
        gfx_init, gfx_destroy, gfx_draw_line, gfx_draw_caret,
        gfx_present, gfx_get_key, gfx_resize,
        gfx_chrome_rows, gfx_draw_gutter, gfx_draw_tab,
        gfx_draw_status, gfx_draw_symbols, gfx_set_theme,
        gfx_capture_shim};
    return &b;
}

/* --- headless capture (no live window needed) ----------------------------
 * Render one frame and read the SDL framebuffer back into an RGBA buffer the
 * caller owns. Used by the screenshot tool to produce pixel-faithful PNGs
 * without ever mapping a window to a display. Returns 0 on success. */
int ui_gfx_capture(void *st, unsigned char **rgba, int *w, int *h) {
    GFX *g = st;
    if (!g || !g->init_ok || !rgba || !w || !h) return -1;
    int ww, wh;
    SDL_GetWindowSize(g->win, &ww, &wh);
    /* force a present so the back buffer holds the last drawn frame */
    SDL_RenderPresent(g->ren);
    unsigned char *pix = malloc((size_t)ww * wh * 4);
    if (!pix) return -1;
    /* SDL_FRAMEBUFFER is RGBA in memory order [R,G,B,A]; the encoder expects
     * exactly that, so no channel swap is needed. */
    if (SDL_RenderReadPixels(g->ren, NULL, SDL_PIXELFORMAT_RGBA32,
                             pix, ww * 4) != 0) {
        free(pix);
        return -1;
    }
    *rgba = pix; *w = ww; *h = wh;
    return 0;
}
