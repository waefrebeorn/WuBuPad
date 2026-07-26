/* ui_headless.c -- null/recording UI backend.
 *
 * Implements the UI_Backend vtable with no real display. Every draw lands in
 * a ring of recorded lines; get_key returns scripted keys from a queue (used by
 * tests to drive an editing session deterministically). This lets the full
 * UI command + render path be exercised under ASan+UBSan with zero platform
 * dependencies, keeping the core honest. Clean C11. */
#include "ui.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    int cols, rows;
    /* recorded last frame: row -> text copy + kind */
    char **lines;
    int   *kinds;
    /* scripted key queue */
    char  *keys;     /* packed: each entry = 1 byte ch + 1 byte key code */
    size_t klen, kpos;
    int    quit;
} HL;

/* push a scripted key (ch bytes, key code). */
static void hl_push_key(HL *h, const char *ch, int key) {
    size_t n = h->klen + 1;
    h->keys = realloc(h->keys, n * 2);
    h->keys[h->klen*2]   = ch ? ch[0] : 0;
    h->keys[h->klen*2+1] = (char)key;
    h->klen = n;
}

static int hl_init(void **st, int cols, int rows) {
    HL *h = calloc(1, sizeof *h);
    if (!h) return -1;
    h->cols = cols > 0 ? cols : 80;
    h->rows = rows > 0 ? rows : 24;
    h->lines = calloc((size_t)h->rows, sizeof(char*));
    h->kinds = calloc((size_t)h->rows, sizeof(int));
    *st = h;
    return 0;
}

static void hl_destroy(void *st) {
    HL *h = st;
    for (int i = 0; i < h->rows; i++) free(h->lines[i]);
    free(h->lines); free(h->kinds); free(h->keys);
    free(h);
}

static void hl_draw_line(void *st, int row, const char *text, int len, int kind) {
    HL *h = st;
    if (row < 0 || row >= h->rows) return;
    free(h->lines[row]);
    int n = len < 0 ? (int)strlen(text ? text : "") : len;
    if (n < 0) n = 0;
    char *buf = malloc((size_t)n + 1);
    if (!buf) return;
    memcpy(buf, text ? text : "", (size_t)n);
    buf[n] = 0;
    h->lines[row] = buf;
    h->kinds[row] = kind;
}

static void hl_draw_caret(void *st, int row, int col) {
    (void)st; (void)row; (void)col;   /* recorded implicitly by test inspection */
}

static void hl_present(void *st) { (void)st; }

static int hl_get_key(void *st, char *ch, int *key) {
    HL *h = st;
    if (h->kpos >= h->klen || h->quit) { *key = UI_KEY_QUIT; *ch = 0; return -1; }
    *ch   = h->keys[h->kpos*2];
    *key  = h->keys[h->kpos*2+1];
    h->kpos++;
    return 0;
}

static void hl_resize(void *st, int *cols, int *rows) {
    HL *h = st;
    if (*cols != h->cols || *rows != h->rows) {
        /* grow/shrink ring */
        char **nl = calloc((size_t)(*rows > 0 ? *rows : 1), sizeof(char*));
        int   *nk = calloc((size_t)(*rows > 0 ? *rows : 1), sizeof(int));
        int copy = (*rows < h->rows) ? *rows : h->rows;
        for (int i = 0; i < copy; i++) { nl[i] = h->lines[i]; nk[i] = h->kinds[i]; }
        for (int i = copy; i < h->rows; i++) free(h->lines[i]);
        free(h->lines); free(h->kinds);
        h->lines = nl; h->kinds = nk; h->rows = *rows > 0 ? *rows : 1;
        h->cols = *cols > 0 ? *cols : 80;
    }
}

const UI_Backend *ui_headless_backend(void) {
    static const UI_Backend b = {
        hl_init, hl_destroy, hl_draw_line, hl_draw_caret, hl_present,
        hl_get_key, hl_resize
    };
    return &b;
}

/* internal (called by ui.c accessors, which hold the complete UI type) */
int  ui__hl_queue(void *bstate, const char *ch, int key) {
    HL *h = bstate;
    if (!h) return -1;
    size_t n = h->klen + 1;
    char *nk = realloc(h->keys, n * 2);
    if (!nk) return -1;
    h->keys = nk;
    h->keys[h->klen*2]   = ch ? ch[0] : 0;
    h->keys[h->klen*2+1] = (char)key;
    h->klen = n;
    return 0;
}
const char *ui__hl_line(void *bstate, int row) {
    HL *h = bstate;
    if (!h || row < 0 || row >= h->rows) return "";
    return h->lines[row] ? h->lines[row] : "";
}
