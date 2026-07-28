/* palette.c -- command palette. See palette.h. */
#include "palette.h"
#include "fuzzy.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PAL_QMAX  256
#define PAL_VMAX  64

struct Palette {
    CommandRegistry *reg;
    int open;
    char query[PAL_QMAX];
    size_t qlen;
    size_t idx[PAL_VMAX];      /* candidate command indices in registry */
    long   score[PAL_VMAX];
    size_t n;                  /* candidate count */
    size_t hi;                 /* highlighted index within candidates */
    char  **names;             /* cached registry names (rebuilt on open) */
    size_t names_n;
};

/* collect registry names into p->names (lazy, on open) */
static int palette__enum_cb(void *ctx, size_t idx, const char *name) {
    Palette *p = ctx;
    p->names[p->names_n] = strdup(name);
    if (p->names[p->names_n]) p->names_n++;
    (void)idx;
    return 0;
}
static void collect_names(Palette *p) {
    /* free old */
    for (size_t i = 0; i < p->names_n; i++) free(p->names[i]);
    free(p->names); p->names = NULL; p->names_n = 0;
    size_t total = command_count(p->reg);
    if (total == 0) return;
    p->names = calloc(total, sizeof(char *));
    if (!p->names) return;
    command_enumerate(p->reg, palette__enum_cb, p);
}

Palette *palette_create(CommandRegistry *reg) {
    if (!reg) return NULL;
    Palette *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    p->reg = reg;
    p->open = 0; p->qlen = 0; p->query[0] = 0; p->hi = 0;
    return p;
}
void palette_free(Palette *p) {
    if (!p) return;
    for (size_t i = 0; i < p->names_n; i++) free(p->names[i]);
    free(p->names);
    free(p);
}

void palette_open(Palette *p) {
    if (!p) return;
    p->open = 1; p->qlen = 0; p->query[0] = 0; p->hi = 0;
    collect_names(p);
    /* initial candidate list = all names */
    p->n = 0;
    for (size_t i = 0; i < p->names_n && p->n < PAL_VMAX; i++) {
        p->idx[p->n] = i; p->score[p->n] = 0; p->n++;
    }
}
void palette_close(Palette *p) { if (p) p->open = 0; }
int  palette_is_open(const Palette *p) { return p ? p->open : 0; }

static void refilter(Palette *p) {
    if (p->qlen == 0) {
        p->n = 0;
        for (size_t i = 0; i < p->names_n && p->n < PAL_VMAX; i++) {
            p->idx[p->n] = i; p->score[p->n] = 0; p->n++;
        }
    } else {
        p->n = fuzzy_top((const char **)p->names, p->names_n, p->query,
                         p->idx, p->score, PAL_VMAX);
    }
    if (p->hi >= p->n) p->hi = 0;
}

int palette_feed(Palette *p, char ch, int key, void *arg) {
    if (!p || !p->open) return -1;
    if (key == 0x1B) { palette_close(p); return -1; }          /* Esc cancel */
    if (ch == '\n') {                                          /* confirm */
        if (p->n == 0) { palette_close(p); return -1; }
        const char *name = p->names[p->idx[p->hi]];
        palette_close(p);
        command_run(p->reg, name, arg);
        return 1;
    }
    if (key == 0x1001) { if (p->hi + 1 < p->n) p->hi++; return 0; } /* down */
    if (key == 0x1002) { if (p->hi > 0) p->hi--;        return 0; } /* up */
    if (ch == '\t')    { if (p->hi + 1 < p->n) p->hi++; return 0; }
    if (ch == '\b' || ch == 127) {                              /* backspace */
        if (p->qlen > 0) p->qlen--;
        p->query[p->qlen] = 0;
        refilter(p);
        return 0;
    }
    if (ch >= 32 && ch < 127 && p->qlen < PAL_QMAX - 1) {       /* printable */
        p->query[p->qlen++] = ch;
        p->query[p->qlen] = 0;
        refilter(p);
        return 0;
    }
    return 0;
}

size_t palette_count(const Palette *p) { return p ? p->n : 0; }
const char *palette_name_at(const Palette *p, size_t i) {
    if (!p || i >= p->n) return NULL;
    return p->names[p->idx[i]];
}
size_t palette_highlight(const Palette *p) { return p ? p->hi : 0; }
const char *palette_query(const Palette *p) { return p ? p->query : ""; }
