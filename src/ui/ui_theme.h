/* ui_theme.h -- semantic theme palette + persistence (Phase C).
 *
 * A theme is a named semantic-token palette (surface/text/accent/selection/
 * line-number/caret + syntax kinds) in two variants: dark and light. The UI
 * flips between them; backends map tokens -> device colors. The active choice
 * is persisted to ~/.wubupad/theme so it survives restarts. Clean C11, opaque. */
#ifndef WUBUPAD_UI_THEME_H
#define WUBUPAD_UI_THEME_H

typedef enum {
    TOK_SURFACE, TOK_SURFACE_ALT, TOK_TEXT, TOK_ACCENT,
    TOK_SELECTION, TOK_LINE_NO, TOK_CARET,
    TOK_KW, TOK_TYPE, TOK_STR, TOK_NUM, TOK_COMMENT, TOK_PREPROC
} UIToken;

#define UI_THEME_NTOK 13

/* RGB triple, 0-255. */
typedef struct { unsigned char r, g, b; } UIRGB;

typedef struct UITheme UITheme;

UITheme *ui_theme_create(void);
void ui_theme_free(UITheme *t);

/* 0 = light, 1 = dark. */
void ui_theme_set_dark(UITheme *t, int dark);
int  ui_theme_is_dark(const UITheme *t);

/* Token color for the active variant. */
UIRGB ui_theme_color(const UITheme *t, UIToken tok);

/* Load/save the active (dark|light) choice from ~/.wubupad/theme.
 * Missing/invalid file -> defaults to dark. */
void ui_theme_load(UITheme *t);
void ui_theme_save(const UITheme *t);

#endif /* WUBUPAD_UI_THEME_H */
