/* multicursor.c -- multiple cursors. See multicursor.h. */
#include "multicursor.h"
#include <stdlib.h>
#include <string.h>

#define MC_MAX 256
struct C { size_t cursor; size_t anchor; };
struct MultiCursor { struct C c[MC_MAX]; size_t n; };

MultiCursor *multicursor_create(void){ return calloc(1, sizeof(MultiCursor)); }
void multicursor_free(MultiCursor *m){ free(m); }
void multicursor_add(MultiCursor *m, size_t cursor, size_t anchor){
    if (!m || m->n >= MC_MAX) return;
    m->c[m->n].cursor = cursor; m->c[m->n].anchor = anchor; m->n++;
}
void multicursor_clear(MultiCursor *m){ if (m) m->n = 0; }
size_t multicursor_count(const MultiCursor *m){ return m ? m->n : 0; }

void multicursor_insert_all(MultiCursor *m, void *buf, mc_insert_fn ins,
                            mc_set_fn set, const char *text, size_t len) {
    if (!m || !ins) return;
    for (size_t i = 0; i < m->n; i++) {
        size_t pos = m->c[i].cursor;   /* already offset for prior inserts */
        ins(buf, pos, text, len);
        /* shift this and every later caret by len */
        for (size_t k = i; k < m->n; k++) {
            if (m->c[k].cursor >= m->c[i].cursor) m->c[k].cursor += len;
            if (m->c[k].anchor >= m->c[i].cursor) m->c[k].anchor += len;
        }
        if (set) set(buf, m->c[i].cursor, m->c[i].anchor);
    }
}
void multicursor_cursors(const MultiCursor *m, size_t *out) {
    if (!m || !out) return;
    for (size_t i = 0; i < m->n; i++) out[i] = m->c[i].cursor;
}
