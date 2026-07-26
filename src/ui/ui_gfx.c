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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H

/* ---- semantic color tokens (Light + Dark) ---- */
typedef enum {
    TOK_SURFACE, TOK_SURFACE_ALT, TOK_TEXT, TOK_ACCENT,
    TOK_SELECTION, TOK_LINE_NO, TOK_CARET,
    TOK_KW, TOK_TYPE, TOK_STR, TOK_NUM, TOK_COMMENT, TOK_PREPROC
} Token;

typedef struct { Uint8 r,g,b; } RGB;
static const RGB LIGHT[] = {
    {255,255,255}, {245,245,245}, {20,20,20},  {0,90,200},
    {180,210,255}, {140,140,140}, {0,0,0},
    {120,30,160},  {0,110,80},     {120,20,20}, {10,90,160}, {110,110,110}, {150,90,0}
};
static const RGB DARK[] = {
    {30,30,34}, {38,38,44}, {220,220,220}, {90,150,255},
    {60,90,140}, {110,110,120}, {255,255,255},
    {200,130,230}, {110,200,160}, {230,150,150}, {120,180,240}, {130,130,140}, {200,150,90}
};

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
    int dark;              /* theme */
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

static const RGB *gfx_palette(GFX *g) { return g->dark ? DARK : LIGHT; }

static void gfx_draw_glyph(GFX *g, int cp, int px, int py, Token tok) {
    int gi = gfx_glyph_index(g, (Uint32)cp);
    if (gi < 0) gi = gfx_cache_glyph(g, (Uint32)(cp ? cp : ' '));
    if (gi < 0) return;
    Glyph *gl = &g->glyphs[gi];
    if (!gl->tex || gl->w <= 0 || gl->h <= 0) return;
    const RGB *pal = gfx_palette(g);
    SDL_SetTextureColorMod(gl->tex, pal[tok].r, pal[tok].g, pal[tok].b);
    SDL_Rect dst = { px + gl->bx, py - gl->by + g->line_h, gl->w, gl->h };
    SDL_RenderCopy(g->ren, gl->tex, NULL, &dst);
}

/* --- vtable --- */
static int gfx_init(void **st, int cols, int rows) {
    GFX *g = calloc(1, sizeof *g);
    if (!g) return -1;
    g->cols = cols > 0 ? cols : 100;
    g->rows = rows > 0 ? rows : 40;
    g->dark = 1;  /* default dark theme */
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
    if (g->face) FT_Done_Face(g->face);
    if (g->ft) FT_Done_FreeType(g->ft);
    if (g->ren) SDL_DestroyRenderer(g->ren);
    if (g->win) SDL_DestroyWindow(g->win);
    SDL_Quit();
    free(g);
}

static void gfx_draw_line(void *st, int row, const char *text, int len, int kind) {
    GFX *g = st;
    if (!g || !g->init_ok || row < 0 || row >= g->rows) return;
    const RGB *pal = gfx_palette(g);
    int py = row * g->line_h;
    /* background */
    SDL_Rect bg = { 0, py, g->cols * g->char_w, g->line_h };
    SDL_SetRenderDrawColor(g->ren, pal[TOK_SURFACE].r, pal[TOK_SURFACE].g,
                           pal[TOK_SURFACE].b, 255);
    SDL_RenderFillRect(g->ren, &bg);

    Token tok = TOK_TEXT;
    switch (kind) {
        case 1: tok = TOK_KW; break;
        case 2: tok = TOK_TYPE; break;
        case 3: tok = TOK_STR; break;
        case 4: tok = TOK_STR; break; /* char */
        case 5: tok = TOK_NUM; break;
        case 6: tok = TOK_COMMENT; break;
        case 7: tok = TOK_PREPROC; break;
        default: tok = TOK_TEXT; break;
    }
    int n = len < 0 ? (int)strlen(text ? text : "") : len;
    int px = 1;
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        gfx_draw_glyph(g, c, px, py, tok);
        px += g->char_w;
    }
}

static void gfx_draw_caret(void *st, int row, int col) {
    GFX *g = st;
    if (!g || !g->init_ok) return;
    const RGB *pal = gfx_palette(g);
    int x = col * g->char_w, y = row * g->line_h;
    SDL_Rect r = { x, y, 2, g->line_h };
    SDL_SetRenderDrawColor(g->ren, pal[TOK_CARET].r, pal[TOK_CARET].g,
                           pal[TOK_CARET].b, 255);
    SDL_RenderFillRect(g->ren, &r);
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
            case SDLK_f: *key = UI_KEY_FIND; return 0;
            case SDLK_a: *key = UI_KEY_HOME; return 0;
            case SDLK_e: *key = UI_KEY_END; return 0;
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
        gfx_present, gfx_get_key, gfx_resize
    };
    return &b;
}
