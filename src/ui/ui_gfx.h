/* ui_gfx.h -- SDL2 + FreeType2 graphics UI backend. */
#ifndef WUBUPAD_UI_GFX_H
#define WUBUPAD_UI_GFX_H

#include "ui.h"

/* The real graphics backend vtable (for ui_create). Requires SDL2 + FreeType2
 * at link time; if those are absent the build excludes ui_gfx.c. */
const UI_Backend *ui_gfx_backend(void);

#endif /* WUBUPAD_UI_GFX_H */
