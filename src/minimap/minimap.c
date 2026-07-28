/* minimap.c -- code minimap. See minimap.h. */
#include "minimap.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct Minimap {
    int rows, cols;
    char *lit;          /* 1 per mini-row */
    size_t n;           /* rows filled */
};

Minimap *minimap_create(int rows, int cols){
    if (rows <= 0 || cols <= 0) return NULL;
    Minimap *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    m->lit = calloc((size_t)rows, 1);
    if (!m->lit) { free(m); return NULL; }
    m->rows = rows; m->cols = cols; m->n = 0;
    return m;
}
void minimap_free(Minimap *m){ if (!m) return; free(m->lit); free(m); }

size_t minimap_update(Minimap *m, const char *text) {
    if (!m || !text) return 0;
    memset(m->lit, 0, (size_t)m->rows);
    size_t r = 0;
    const char *p = text;
    while (*p && r < (size_t)m->rows) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        int has = 0;
        for (size_t i = 0; i < len; i++)
            if (!isspace((unsigned char)p[i])) { has = 1; break; }
        m->lit[r] = has ? 1 : 0;
        r++;
        if (!nl) break;
        p = nl + 1;
    }
    m->n = r;
    return r;
}
int minimap_lit(const Minimap *m, size_t r){ return m && r < m->n && m->lit[r]; }
size_t minimap_lit_count(const Minimap *m){
    if (!m) return 0; size_t c = 0;
    for (size_t i = 0; i < m->n; i++) c += m->lit[i];
    return c;
}
size_t minimap_rows(const Minimap *m){ return m ? m->n : 0; }
