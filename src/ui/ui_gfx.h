/* ui_gfx.h -- SDL2 + FreeType2 graphics UI backend. */
#ifndef WUBUPAD_UI_GFX_H
#define WUBUPAD_UI_GFX_H

#include "ui.h"

/* The real graphics backend vtable (for ui_create). Requires SDL2 + FreeType2
 * at link time; if those are absent the build excludes ui_gfx.c. */
const UI_Backend *ui_gfx_backend(void);

/* Headless frame capture (no live window): render one frame and read the SDL
 * framebuffer into a malloc'd RGBA buffer the caller frees. Returns 0 ok.
 * Only meaningful on the gfx backend (SDL2 present). */
int ui_gfx_capture(void *st, unsigned char **rgba, int *w, int *h);

#endif /* WUBUPAD_UI_GFX_H */
