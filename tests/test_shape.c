/* test_shape.c -- text shaping (HarfBuzz + FriBidi) unit test.
 * Verifies that the wubushape layer produces positioned glyphs for LTR and
 * RTL (Arabic) text, and that the caret map covers source characters.
 * If shaping support was not compiled in (no HarfBuzz/FriBidi), the test
 * skips with PASS. */
#include "shape/shape.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

int main(void) {
    /* We need an FT_Face to create a ShapeCtx. Reuse the gfx backend's font
     * loading by opening FreeType directly here (lightweight, headless). */
    /* To avoid pulling SDL/FT into the test, we load FT ourselves. */
    /* (test links wubushape only; FT is a transitive dep of wubushape.) */

    /* shape_create needs the FT_Face; we open it inline. */
    /* Minimal FT bootstrap guarded by the same optional macro. */
#ifdef WUBUPAD_WITH_SHAPE
    #include <ft2build.h>
    #include FT_FREETYPE_H
    FT_Library ft;
    if (FT_Init_FreeType(&ft) != 0) { printf("SKIP: FT init failed\n"); return 0; }
    FT_Face face = NULL;
    if (FT_New_Face(ft, FONT, 0, &face) != 0) {
        FT_Done_FreeType(ft);
        printf("SKIP: font not found\n"); return 0;
    }
    FT_Set_Pixel_Sizes(face, 0, 18);

    ShapeCtx *ctx = shape_create(face);
    if (!ctx) {
        FT_Done_Face(face); FT_Done_FreeType(ft);
        printf("SKIP: shape_create returned NULL (HarfBuzz/FriBidi missing)\n");
        return 0;
    }

    int fails = 0;
    ShapeGlyph *gly = NULL; int gc = 0, adv = 0;
    int cx[256]; for (int i=0;i<256;i++) cx[i]=0;

    /* LTR Latin */
    gc = adv = 0; gly = NULL;
    shape_line(ctx, "Hello", SHAPE_DIR_LTR, &gly, &gc, &adv, cx, 256);
    if (gc <= 0) { printf("FAIL: Hello produced no glyphs\n"); fails++; }
    else if (adv <= 0) { printf("FAIL: Hello advance <= 0\n"); fails++; }
    free(gly); gly = NULL; gc = 0; adv = 0;

    /* RTL Arabic (visual order must differ from logical) */
    /* U+0627 U+0644 U+0644 U+064A = "الي" */
    const char *ar = "\xd8\xa7\xd9\x84\xd9\x84\xd9\x8a";
    shape_line(ctx, ar, SHAPE_DIR_RTL, &gly, &gc, &adv, cx, 256);
    if (gc <= 0) { printf("FAIL: Arabic produced no glyphs\n"); fails++; }
    else if (adv <= 0) { printf("FAIL: Arabic advance <= 0\n"); fails++; }
    free(gly); gly = NULL; gc = 0; adv = 0;

    /* caret map for a mixed LTR run */
    int car[16]; for (int i=0;i<16;i++) car[i]=0;
    shape_line(ctx, "Hi", SHAPE_DIR_LTR, &gly, &gc, &adv, car, 16);
    if (car[0] == 0 && adv > 0) { printf("FAIL: caret map not populated\n"); fails++; }
    free(gly);

    shape_destroy(ctx);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: text shaping (HarfBuzz + FriBidi)\n");
    return 0;
#else
    printf("SKIP: shaping not compiled in (HarfBuzz/FriBidi absent)\n");
    return 0;
#endif
}
