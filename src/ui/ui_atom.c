/* ui_atom.c -- Atom subsystem wiring for the WuBuPad UI controller.
 *
 * Owns the global CommandRegistry + command Palette (the Atom spine) and the
 * PackageManager as module-level state (the app drives a single UI). ui.c
 * calls ui_atom_init()/ui_atom_free() across the UI lifetime and routes
 * keystrokes to the palette while it is open. Built-in editor commands are
 * registered so the palette lists + runs them, exactly like Atom packages.
 * The live UI* is stashed as g_ui so command callbacks can mutate the editor.
 * Clean C11, opaque (never reaches into struct UI internals). */
#include "ui_atom.h"
#include "command.h"
#include "fuzzy.h"
#include "palette.h"
#include "pkgmgr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef WUBUPAD_PKGS_DIR
#define WUBUPAD_PKGS_DIR ".wubupad/packages"
#endif

/* module state (single active UI in the app) */
static CommandRegistry *g_reg = NULL;
static Palette        *g_pal = NULL;
static PackageManager *g_pkg = NULL;
static UI             *g_ui = NULL;   /* stashed for command callbacks */

/* ---- built-in commands (editor:*, tree-view:*, snippets:*, etc.) ---- */
static int cmd_toggle_theme(void *arg){
    UI *ui = arg; if (!ui) return -1; ui_toggle_theme(ui); return 0;
}
static int cmd_toggle_tree(void *arg){
    UI *ui = arg; if (!ui) return -1; ui_toggle_tree(ui); return 0;
}
static int cmd_toggle_symbols(void *arg){
    UI *ui = arg; if (!ui) return -1; ui_toggle_symbols(ui); return 0;
}
static int cmd_toggle_fold(void *arg){
    UI *ui = arg; if (!ui) return -1; ui_fold_current_block(ui); return 0;
}
static int cmd_open_palette_again(void *arg){
    UI *ui = arg; if (!ui) return -1; ui_open_palette(ui); return 0;
}
static int cmd_convert_eol(void *arg){
    UI *ui = arg; if (!ui) return -1; ui_convert_eol(ui); return 0;
}

void ui_atom_init(UI *ui) {
    g_ui = ui;
    g_reg = command_registry_create();
    g_pal = palette_create(g_reg);
    char dir[1024];
    const char *home = getenv("HOME");
    if (home) snprintf(dir, sizeof dir, "%s/%s", home, WUBUPAD_PKGS_DIR);
    else      snprintf(dir, sizeof dir, "./%s", WUBUPAD_PKGS_DIR);
    g_pkg = pkgmgr_create(g_reg, dir);
    if (g_pkg) pkgmgr_discover(g_pkg);
    command_register(g_reg, "editor:toggle-theme", cmd_toggle_theme);
    command_register(g_reg, "editor:toggle-tree-view", cmd_toggle_tree);
    command_register(g_reg, "editor:toggle-function-list", cmd_toggle_symbols);
    command_register(g_reg, "editor:fold-current-block", cmd_toggle_fold);
    command_register(g_reg, "command:palette", cmd_open_palette_again);
    command_register(g_reg, "editor:convert-eol", cmd_convert_eol);
}

void ui_atom_free(UI *ui) {
    (void)ui;
    if (g_pal) palette_free(g_pal);
    if (g_pkg) pkgmgr_free(g_pkg);
    if (g_reg) command_registry_free(g_reg);
    g_pal = NULL; g_pkg = NULL; g_reg = NULL; g_ui = NULL;
}

CommandRegistry *ui_command_registry(const UI *ui){ (void)ui; return g_reg; }
int ui_palette_open(const UI *ui){ (void)ui; return g_pal && palette_is_open(g_pal); }

void ui_open_palette(UI *ui){
    if (!g_pal) return;
    g_ui = ui;
    palette_open(g_pal);
}

/* feed one key to the palette; returns 1 if it consumed the event. */
int ui_palette_feed(UI *ui, char ch, int key) {
    if (!g_pal || !palette_is_open(g_pal)) return 0;
    g_ui = ui;
    palette_feed(g_pal, ch, key, g_ui);
    return 1;
}

size_t ui_palette_render(const UI *ui, char *buf, size_t bufn) {
    (void)ui;
    if (!g_pal || !palette_is_open(g_pal) || !buf || bufn == 0) return 0;
    size_t n = palette_count(g_pal);
    size_t hl = palette_highlight(g_pal);
    size_t wrote = 0;
    for (size_t i = 0; i < n && wrote < bufn; i++) {
        const char *name = palette_name_at(g_pal, i);
        if (!name) continue;
        int room = (int)(bufn - wrote);
        int used = snprintf(buf + wrote, (size_t)room, "%c %s\n",
                            (i == hl) ? '>' : ' ', name);
        if (used < 0) break;
        wrote += (size_t)used;
        if (wrote >= bufn) { wrote = bufn; break; }
    }
    if (wrote < bufn) buf[wrote] = 0; else buf[bufn-1] = 0;
    return wrote;
}
