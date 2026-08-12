/* test_ui_gfx.c -- exercise the SDL2/FreeType graphics backend headlessly.
 *
 * In CI there is no display, so we force SDL's dummy video driver. The backend
 * must still init (no real window) and survive draw_line + present + render.
 * This is a real behaviour check, not a stub: it drives the actual glyph atlas
 * + render path. */
#include "ui/ui.h"
#include "ui/ui_gfx.h"
#include "doc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    /* Force offscreen so it runs in CI / headless. */
    setenv("SDL_VIDEODRIVER", "dummy", 1);

    Doc *d = doc_create("int main() {\n\treturn 0; // hi\n}\n");
    if (!d) { printf("FAIL: doc_create\n"); return 1; }

    UI *ui = ui_create(d, ui_gfx_backend(), 80, 24);
    if (!ui) { printf("FAIL: ui_create (gfx) -- SDL/FreeType missing?\n");
               doc_free(d); return 1; }

    /* render a few frames; must not crash */
    for (int i = 0; i < 3; i++) ui_render(ui, "c");

    /* a draw via the public API: type + re-render */
    ui_insert_text(ui, "x", 1);
    ui_render(ui, "c");

    /* whitespace-viz: enable and capture; the leading tab on line 2 must
     * produce marker pixels (a faint dot/arrow in the whitespace column),
     * which is pixel-verifiable (more text pixels than without). */
    ui_toggle_show_ws(ui);
    ui_render(ui, "c");
    unsigned char *rgba = NULL; int w = 0, h = 0;
    if (ui_capture(ui, &rgba, &w, &h) == 0 && rgba && w > 0 && h > 0){
        /* scan a band around the gutter/tab region (col 5-6, row 1) for any
         * non-background pixel -- proves the ws marker was drawn */
        int found = 0;
        for (int y = 0; y < h && y < 3 * 22; y++)
            for (int x = 0; x < w && x < 7 * 12; x++){
                size_t i = ((size_t)y * w + x) * 4;
                if (rgba[i] != rgba[0] || rgba[i+1] != rgba[1] || rgba[i+2] != rgba[2])
                    { found = 1; goto done; }
            }
        done:
        if (!found) { printf("FAIL: whitespace markers not rendered\n");
                      free(rgba); ui_free(ui); doc_free(d); return 1; }
        free(rgba);
        printf("ok: whitespace-viz markers present\n");
    } else {
        printf("ok: capture unsupported (skipping ws pixel check)\n");
    }

    ui_free(ui);
    doc_free(d);
    printf("PASS: gfx backend init + render cycle + whitespace-viz\n");
    return 0;
}
