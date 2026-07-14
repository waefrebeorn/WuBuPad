/* docs.c -- multi-document session. Reuses Doc; owns metadata + ordering. */
#include "docs.h"
#include "encode.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

typedef struct {
    Doc    *doc;
    char   *path;     /* malloc'd or NULL */
    char   *lang;     /* malloc'd or NULL */
    int     dirty;
} Slot;

struct Docs {
    Slot  *slots;
    size_t n, cap;
    size_t active;
};

static void *xrealloc(void *p, size_t n) { void *r = realloc(p, n?n:1); if(!r) abort(); return r; }
static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *r = xrealloc(NULL, n);
    memcpy(r, s, n);
    return r;
}

Docs *docs_create(void) {
    Docs *s = xrealloc(NULL, sizeof *s);
    memset(s, 0, sizeof *s);
    return s;
}

void docs_free(Docs *s) {
    if (!s) return;
    for (size_t i = 0; i < s->n; i++) {
        doc_free(s->slots[i].doc);
        free(s->slots[i].path);
        free(s->slots[i].lang);
    }
    free(s->slots);
    free(s);
}

size_t docs_open(Docs *s, const char *path, const char *text, const char *lang) {
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->slots = xrealloc(s->slots, s->cap * sizeof *s->slots);
    }
    size_t i = s->n++;
    s->slots[i].doc   = doc_create(text);
    s->slots[i].path  = xstrdup(path);
    s->slots[i].lang  = xstrdup(lang);
    /* A doc that has a path is considered clean (it mirrors a file); an
     * untitled/empty buffer is also clean until edited. */
    s->slots[i].dirty = 0;
    s->active = i;
    return i;
}

int docs_close(Docs *s, size_t i) {
    if (i >= s->n) return 0;
    doc_free(s->slots[i].doc);
    free(s->slots[i].path);
    free(s->slots[i].lang);
    memmove(&s->slots[i], &s->slots[i+1], (s->n - i - 1) * sizeof *s->slots);
    s->n--;
    if (s->active >= s->n) s->active = s->n ? s->n - 1 : 0;
    return 1;
}

size_t docs_count(const Docs *s) { return s->n; }
size_t docs_active(const Docs *s) { return s->active; }
void   docs_set_active(Docs *s, size_t i) { if (i < s->n) s->active = i; }
Doc   *docs_doc(Docs *s, size_t i) { return (i < s->n) ? s->slots[i].doc : NULL; }
const char *docs_path(const Docs *s, size_t i) { return (i < s->n) ? s->slots[i].path : NULL; }
const char *docs_lang(const Docs *s, size_t i) { return (i < s->n) ? s->slots[i].lang : NULL; }
int        docs_dirty(const Docs *s, size_t i) { return (i < s->n) ? s->slots[i].dirty : 0; }
void docs_set_dirty(Docs *s, size_t i, int dirty) { if (i < s->n) s->slots[i].dirty = dirty; }
void docs_set_path(Docs *s, size_t i, const char *path) {
    if (i >= s->n) return;
    free(s->slots[i].path);
    s->slots[i].path = xstrdup(path);
}

size_t docs_load_file(Docs *s, const char *path, const char *lang) {
    FILE *f = fopen(path, "rb");
    if (!f) return SIZE_MAX;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *raw = xrealloc(NULL, (sz > 0 ? (size_t)sz : 1));
    if (fread(raw, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(raw); return SIZE_MAX; }
    fclose(f);
    size_t ulen = 0;
    char *text = enc_decode(raw, (size_t)sz, &ulen);
    free(raw);
    if (!text) return SIZE_MAX;
    size_t idx = docs_open(s, path, text, lang);
    free(text);
    if (idx != SIZE_MAX) docs_set_dirty(s, idx, 0);  /* loaded = clean */
    return idx;
}

int docs_save_file(Docs *s, size_t i) {
    if (i >= s->n) return -1;
    const char *path = s->slots[i].path;
    if (!path) return -1;
    Doc *d = s->slots[i].doc;
    char *text = doc_text(d);       /* UTF-8, NUL-terminated */
    size_t len = strlen(text);
    FILE *f = fopen(path, "wb");
    if (!f) { free(text); return -1; }
    int ok = (fwrite(text, 1, len, f) == len);
    free(text);
    if (fclose(f) != 0) ok = 0;
    if (ok) docs_set_dirty(s, i, 0);
    return ok ? 0 : -1;
}
