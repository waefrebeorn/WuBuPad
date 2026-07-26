/* ui_tty.c -- minimal curses-free terminal backend for the UI layer.
 *
 * Uses raw ANSI escapes (no ncurses, no toolkit fork). Provides a real,
 * interactive editing surface: clears the screen, paints each viewport row,
 * draws a block caret, and reads keystrokes via termios raw mode. This makes
 * WuBuPad demonstrably usable as an editor before the SDL2/FreeType graphics
 * backend lands. Clean C11, platform bits isolated here only. */
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

typedef struct {
    int cols, rows;
    struct termios saved;
    int raw;
} TTY;

static int tty_init(void **st, int cols, int rows) {
    TTY *t = calloc(1, sizeof *t);
    if (!t) return -1;
    t->cols = cols > 0 ? cols : 80;
    t->rows = rows > 0 ? rows : 24;
    *st = t;
    return 0;
}

static void tty_destroy(void *st) {
    TTY *t = st;
    if (t->raw) { tcsetattr(STDIN_FILENO, TCSANOW, &t->saved); }
    /* clear to end + show cursor */
    fputs("\x1b[?25h\x1b[0m", stdout);
    fflush(stdout);
    free(t);
}

static void tty_draw_line(void *st, int row, const char *text, int len, int kind) {
    TTY *t = st;
    (void)kind;
    if (row < 0 || row >= t->rows) return;
    /* colour by token kind (basic) */
    const char *col = "\x1b[0m";
    switch (kind) {
        case 1:  col = "\x1b[35m"; break; /* keyword */
        case 2:  col = "\x1b[36m"; break; /* type */
        case 3:  col = "\x1b[32m"; break; /* string */
        case 5:  col = "\x1b[33m"; break; /* number */
        case 6:  col = "\x1b[90m"; break; /* comment */
        default: col = "\x1b[0m";  break;
    }
    int n = len < 0 ? (int)strlen(text ? text : "") : len;
    printf("\x1b[%d;1H\x1b[K%s%.*s\x1b[0m", row + 1, col, n, text ? text : "");
    (void)t;
}

static void tty_draw_caret(void *st, int row, int col) {
    TTY *t = st;
    if (row < 0 || row >= t->rows) return;
    printf("\x1b[%d;%dH\x1b[?25h", row + 1, col + 1);
    (void)t;
}

static void tty_present(void *st) {
    (void)st;
    fflush(stdout);
}

static int tty_get_key(void *st, char *ch, int *key) {
    TTY *t = st;
    (void)t;
    char buf[4];
    int n = (int)read(STDIN_FILENO, buf, sizeof buf);
    if (n <= 0) { *key = UI_KEY_QUIT; *ch = 0; return -1; }
    if (buf[0] == 27) {  /* escape sequence */
        if (n >= 3 && buf[1] == '[') {
            switch (buf[2]) {
                case 'A': *key = UI_KEY_UP;    break;
                case 'B': *key = UI_KEY_DOWN;  break;
                case 'C': *key = UI_KEY_RIGHT; break;
                case 'D': *key = UI_KEY_LEFT;  break;
                case 'H': *key = UI_KEY_HOME;  break;
                case 'F': *key = UI_KEY_END;   break;
                case '5': *key = UI_KEY_PGUP;  break;
                case '6': *key = UI_KEY_PGDOWN;break;
                default:   *key = UI_KEY_NONE; break;
            }
        } else if (n == 1) { *key = UI_KEY_QUIT; }  /* bare ESC -> quit */
        *ch = 0;
        return 0;
    }
    *ch = buf[0];
    switch (buf[0]) {
        case 127: case 8: *key = UI_KEY_BACKSPACE; break;
        case 10:  case 13: *key = UI_KEY_ENTER;     break;
        case 1:   *key = UI_KEY_HOME;  break;  /* ctrl-a */
        case 5:   *key = UI_KEY_END;   break;  /* ctrl-e */
        case 11:  *key = UI_KEY_PGUP;  break;  /* ctrl-k (rough) */
        case 21:  *key = UI_KEY_PGDOWN;break;  /* ctrl-u */
        case 25:  *key = UI_KEY_REDO;  break;  /* ctrl-y */
        case 26:  *key = UI_KEY_UNDO;  break;  /* ctrl-z */
        case 24:  *key = UI_KEY_COLMODE;break; /* ctrl-x column mode */
        case 7:   *key = UI_KEY_EOL;    break; /* ctrl-g EOL convert */
        case 2:   *key = UI_KEY_MACRO;  break; /* ctrl-b macro toggle */
        case 14:  *key = UI_KEY_REPLAY; break; /* ctrl-n replay macro */
        default:  *key = UI_KEY_NONE;  break;
    }
    return 0;
}

static void tty_resize(void *st, int *cols, int *rows) {
    TTY *t = st;
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col && ws.ws_row) {
        *cols = ws.ws_col; *rows = ws.ws_row;
    }
    t->cols = *cols; t->rows = *rows;
}

const UI_Backend *ui_tty_backend(void) {
    static const UI_Backend b = {
        .init = tty_init, .destroy = tty_destroy,
        .draw_line = tty_draw_line, .draw_caret = tty_draw_caret,
        .present = tty_present, .get_key = tty_get_key, .resize = tty_resize,
        .chrome_rows = NULL, .draw_gutter = NULL, .draw_tab = NULL,
        .draw_status = NULL, .set_theme = NULL
    };
    return &b;
}

/* internal: flip the terminal to raw mode (called by ui.c's ui_tty_enable_raw,
 * which holds the complete UI type). */
int ui__tty_enable_raw(void *bstate) {
    TTY *t = bstate;
    if (!t) return -1;
    if (tcgetattr(STDIN_FILENO, &t->saved) != 0) return -1;
    struct termios raw = t->saved;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return -1;
    t->raw = 1;
    fputs("\x1b[?25l\x1b[2J", stdout);   /* hide cursor, clear */
    fflush(stdout);
    return 0;
}
