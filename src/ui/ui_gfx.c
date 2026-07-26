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
#include "ui_theme.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H

/* Token kinds used by draw_line's `kind` arg (mirrors LexTok from lex.h). */
typedef enum {
    TKN_PLAIN = 0, TKN_KW, TKN_TYPE, TKN_STR, TKN_CHAR,
    TKN_NUM, TKN_COMMENT, TKN_PREPROC
} GfxTok;

#define FONT_PT 18

typedef struct {
    SDL_Texture *tex;     /* per-glyph texture (works on any driver) */
    int w, h;             /* glyph bitmap size */
    int bx, by;           /* glyph bbox offset */
    int ax;               /* advance x */
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
} GFX;

/* --- glyph cache (per-glyph texture; no render-target needed) --- */
static int gfx_glyph_index(GFX *g, Uint32 cp) {
    /* cp stashed in bx field for O(1) find via linear scan (editor glyph set small) */
    for (int i = 0; i < g->nglyphs; i++)
        if ((Uint32)g->glyphs[i].bx == cp) return i;
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

static UIRGB gfx_color(GFX *g, UIToken tok) {
    return ui_theme_color(g->theme, tok);
}

static void gfx_draw_glyph(GFX *g, int cp, int px, int py, UIToken tok) {
    int gi = gfx_glyph_index(g, (Uint32)cp);
    if (gi < 0) gi = gfx_cache_glyph(g, (Uint32)(cp ? cp : ' '));
    if (gi < 0) return;
    Glyph *gl = &g->glyphs[gi];
    if (!gl->tex || gl->w <= 0 || gl->h <= 0) return;
    UIRGB c = gfx_color(g, tok);
    SDL_SetTextureColorMod(gl->tex, c.r, c.g, c.b);
    SDL_Rect dst = { px + gl->bx, py - gl->by + g->line_h, gl->w, gl->h };
    SDL_RenderCopy(g->ren, gl->tex, NULL, &dst);
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
    /* default monospace; fall back to any if missing */
    const char *font = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
    if (FT_New_Face(g->ft, font, 0, &g->face) != 0) {
        font = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
        if (FT_New_Face(g->ft, font, 0, &g->face) != 0) {
            fprintf(stderr, "gfx_init: FT_New_Face failed for %s\n", font);
            FT_Done_FreeType(g->ft); SDL_DestroyRenderer(g->ren);
            SDL_DestroyWindow(g->win); SDL_Quit(); free(g); return -1;
        }
    }
    FT_Set_Pixel_Sizes(g->face, 0, FONT_PT);

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
    gfx_fill(g, 0, py, g->cols * g->char_w, g->line_h, TOK_SURFACE);
    UIToken tok = (UIToken)gfx_kind_token(kind);
    int n = len < 0 ? (int)strlen(text ? text : "") : len;
    int px = g->char_w * 5 + 2;   /* leave room for the gutter */
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        gfx_draw_glyph(g, c, px, py, tok);
        px += g->char_w;
    }
}

static void gfx_draw_caret(void *st, int row, int col) {
    GFX *g = st;
    if (!g || !g->init_ok) return;
    int x = g->char_w * 5 + 2 + col * g->char_w, y = row * g->line_h;
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
    gfx_fill(g, 0, py, g->char_w * 5, g->line_h, TOK_SURFACE_ALT);
    char buf[16];
    int n = snprintf(buf, sizeof buf, "%d", line_no);
    int px = 2;
    for (int i = 0; i < n && i < 5; i++)
        gfx_draw_glyph(g, (unsigned char)buf[i], px + i * g->char_w, py, TOK_LINE_NO);
}

static void gfx_draw_tab(void *st, int tab_row, int index,
                         const char *name, int active, int dirty) {
    GFX *g = st;
    if (!g || !g->init_ok || tab_row != 0) return;
    int x = index * g->char_w * 20;       /* each tab ~20 chars wide */
    int w = g->char_w * 20;
    gfx_fill(g, x, 0, w, g->line_h, active ? TOK_ACCENT : TOK_SURFACE_ALT);
    char buf[20];
    int n = snprintf(buf, sizeof buf, active ? "[%s%s]" : " %s%s ",
                     name ? name : "", dirty ? "*" : "");
    int px = x + 4;
    for (int i = 0; i < n && px < x + w - 4; i++)
        gfx_draw_glyph(g, (unsigned char)buf[i], px + i * g->char_w, 0,
                       active ? TOK_TEXT : TOK_LINE_NO);
}

static void gfx_draw_status(void *st, int row, const char *text) {
    GFX *g = st;
    if (!g || !g->init_ok || row != g->rows - 1) return;
    int py = row * g->line_h;
    gfx_fill(g, 0, py, g->cols * g->char_w, g->line_h, TOK_SURFACE_ALT);
    int n = (int)strlen(text ? text : "");
    int px = 4;
    for (int i = 0; i < n; i++)
        gfx_draw_glyph(g, (unsigned char)text[i], px + i * g->char_w, py, TOK_TEXT);
}

static void gfx_set_theme(void *st, int dark) {
    GFX *g = st;
    if (!g || !g->theme) return;
    ui_theme_set_dark(g->theme, dark);
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
            case SDLK_a: *key = UI_KEY_HOME; return 0;
            case SDLK_t: *key = (mod & KMOD_SHIFT) ? UI_KEY_PREVTAB : UI_KEY_NEXTTAB; return 0;
            case SDLK_c: *key = (mod & KMOD_SHIFT) ? UI_KEY_COLMODE : UI_KEY_NONE; return 0;
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

const UI_Backend *ui_gfx_backend(void) {
    static const UI_Backend b = {
        gfx_init, gfx_destroy, gfx_draw_line, gfx_draw_caret,
        gfx_present, gfx_get_key, gfx_resize,
        gfx_chrome_rows, gfx_draw_gutter, gfx_draw_tab,
        gfx_draw_status, gfx_set_theme
    };
    return &b;
}
