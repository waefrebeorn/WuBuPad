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

    ui_free(ui);
    doc_free(d);
    printf("PASS: gfx backend init + render cycle\n");
    return 0;
}
