/* test_atom.c -- integration: command palette drives the UI controller. */
#include "ui/ui.h"
#include "ui/ui_atom.h"
#include "command/command.h"
#include "doc.h"
#include "ui/ui_headless.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"FAIL: %s\n",(m)); fails++; } }while(0)

int main(void){
    Doc *d = doc_create("hello world\n");
    UI *ui = ui_create(d, ui_headless_backend(), 80, 24);
    CK(ui, "ui_create");
    CK(ui_command_registry(ui) != NULL, "registry exists");
    CK(command_count(ui_command_registry(ui)) >= 6, "built-in commands registered");

    /* open the palette via the UI key, then type "theme" and confirm */
    ui_apply(ui, 0, UI_KEY_PALETTE);
    CK(ui_palette_open(ui), "palette open after key");
    const char *q = "theme";
    for (size_t i=0;q[i];i++) ui_apply(ui, q[i], 0);
    /* the highlight should be editor:toggle-theme (best fuzzy match) */
    char buf[4096];
    size_t n = ui_palette_render(ui, buf, sizeof buf);
    CK(n > 0, "palette rendered lines");
    CK(strstr(buf, "editor:toggle-theme") != NULL, "theme command listed");

    /* confirm with Enter: runs command_toggle_theme -> dark flips */
    int before = ui_theme_dark(ui);
    ui_apply(ui, '\n', UI_KEY_NONE);
    CK(!ui_palette_open(ui), "palette closed after confirm");
    CK(ui_theme_dark(ui) != before, "theme toggled via palette command");

    /* Esc cancels (ESC delivered as key code, matching palette_feed) */
    ui_apply(ui, 0, UI_KEY_PALETTE);
    ui_apply(ui, 0, 0x1B);   /* ESC as key */
    CK(!ui_palette_open(ui), "esc closes palette");

    ui_free(ui); doc_free(d);
    if (fails){ printf("ATOM TESTS FAILED (%d)\n",fails); return 1; }
    printf("ATOM TESTS PASSED\n"); return 0;
}
