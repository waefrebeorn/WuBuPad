/* ui_theme.c -- semantic theme palette + persistence (see ui_theme.h). */
#include "ui_theme.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Dark + Light palettes indexed by UIToken. */
static const UIRGB DARK[UI_THEME_NTOK] = {
    {30,30,34}, {38,38,44}, {220,220,220}, {90,150,255},
    {60,90,140}, {110,110,120}, {255,255,255},
    {200,130,230}, {110,200,160}, {230,150,150}, {120,180,240}, {130,130,140}, {200,150,90}
};
static const UIRGB LIGHT[UI_THEME_NTOK] = {
    {255,255,255}, {245,245,245}, {20,20,20}, {0,90,200},
    {180,210,255}, {140,140,140}, {0,0,0},
    {120,30,160}, {0,110,80}, {120,20,20}, {10,90,160}, {110,110,110}, {150,90,0}
};

struct UITheme { int dark; };

UITheme *ui_theme_create(void) {
    UITheme *t = calloc(1, sizeof *t);
    if (t) t->dark = 1;  /* default dark */
    return t;
}
void ui_theme_free(UITheme *t) { free(t); }

void ui_theme_set_dark(UITheme *t, int dark) { if (t) t->dark = dark ? 1 : 0; }
int  ui_theme_is_dark(const UITheme *t) { return t ? t->dark : 1; }

UIRGB ui_theme_color(const UITheme *t, UIToken tok) {
    int i = (int)tok;
    if (i < 0 || i >= UI_THEME_NTOK) i = (int)TOK_TEXT;
    return t && t->dark ? DARK[i] : LIGHT[i];
}

void ui_theme_load(UITheme *t) {
    if (!t) return;
    char path[512];
    const char *home = getenv("HOME");
    snprintf(path, sizeof path, "%s/.wubupad/theme", home ? home : ".");
    FILE *f = fopen(path, "r");
    if (!f) return;  /* default dark */
    char buf[32];
    if (fgets(buf, sizeof buf, f)) {
        if (strncmp(buf, "light", 5) == 0) t->dark = 0;
        else if (strncmp(buf, "dark", 4) == 0) t->dark = 1;
    }
    fclose(f);
}

void ui_theme_save(const UITheme *t) {
    if (!t) return;
    char path[512];
    const char *home = getenv("HOME");
    snprintf(path, sizeof path, "%s/.wubupad", home ? home : ".");
    /* mkdir -p the dir (best-effort) */
    mkdir(path, 0755);  /* POSIX; ignore EEXIST */
    snprintf(path, sizeof path, "%s/.wubupad/theme", home ? home : ".");
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(t->dark ? "dark\n" : "light\n", f);
    fclose(f);
}
