/* ui_theme.h -- semantic theme palette + persistence (Phase C, GUI overhaul).
 *
 * A theme is a named semantic-token palette in two variants: dark and light.
 * Tokens follow the WuBu GUI Spec (ref/GUI_SPEC.md §1): surfaces are
 * differentiated by LIGHTNESS, not hue; one brand accent; text on every
 * surface meets WCAG 4.5:1 (body) / 3:1 (large + non-text). Backends map
 * tokens -> device colors. The active choice persists to ~/.wubupad/theme.
 * Clean C11, opaque. */
#ifndef WUBUPAD_UI_THEME_H
#define WUBUPAD_UI_THEME_H

typedef enum {
    /* Surfaces (backgrounds), ordered darkest -> lightest */
    TOK_SURFACE,        /* app / editor background            */
    TOK_SURFACE_2,      /* chrome: tab bar, gutter, status    */
    TOK_SURFACE_3,      /* raised: active tab, dialogs, toasts */
    TOK_SURFACE_HOVER,  /* hover state                        */
    TOK_SURFACE_ACTIVE, /* selected / pressed                 */
    /* Foreground (text + lines) */
    TOK_TEXT,           /* primary body text                  */
    TOK_TEXT_DIM,       /* secondary: line numbers, hints     */
    TOK_TEXT_FAINT,     /* disabled                           */
    /* Accent + selection */
    TOK_ACCENT,         /* the single brand accent            */
    TOK_ON_ACCENT,      /* text/icon on accent fill           */
    TOK_ACCENT_SOFT,    /* selection / active-row tint        */
    /* Lines / structure */
    TOK_BORDER,         /* separators, outlines               */
    TOK_CARET,          /* text cursor                        */
    /* Syntax kinds */
    TOK_KW, TOK_TYPE, TOK_STR, TOK_NUM, TOK_COMMENT, TOK_PREPROC
} UIToken;

#define UI_THEME_NTOK 21

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
