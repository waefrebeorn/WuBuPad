/* snippet.c -- snippets engine. See snippet.h. */
#include "snippet.h"
#include <stdlib.h>
#include <string.h>

#define SN_TRIG 64
#define SN_BODY 4096
#define SN_MAXTS 64

struct Snip { char trig[SN_TRIG]; char body[SN_BODY]; };

struct TS { size_t start; size_t end; int id; };

struct SnippetEngine {
    struct Snip items[256];
    size_t n;
    int active;                 /* expansion in progress */
    struct TS ts[SN_MAXTS];     /* tabstops (sorted by start) */
    int  ts_n;
    int  cur;                   /* current tabstop index */
};

SnippetEngine *snippet_create(void){ return calloc(1, sizeof(SnippetEngine)); }
void snippet_free(SnippetEngine *e){ free(e); }

int snippet_add(SnippetEngine *e, const char *trigger, const char *body) {
    if (!e || !trigger || !body) return -1;
    if (e->n >= 256) return -1;
    strncpy(e->items[e->n].trig, trigger, SN_TRIG - 1);
    strncpy(e->items[e->n].body, body, SN_BODY - 1);
    e->n++;
    return 0;
}

static const struct Snip *find(SnippetEngine *e, const char *trigger) {
    for (size_t i = 0; i < e->n; i++)
        if (strcmp(e->items[i].trig, trigger) == 0) return &e->items[i];
    return NULL;
}

int snippet_expand(SnippetEngine *e, void *buf, snip_insert_fn ins,
                   snip_delete_fn del, snip_len_fn lenfn,
                   const char *trigger, size_t pos) {
    if (!e) return -1;
    const struct Snip *s = find(e, trigger);
    if (!s) return 0;
    /* parse body into literal text + tabstops */
    e->ts_n = 0; e->cur = 0; e->active = 0;
    const char *p = s->body; size_t out = pos; int last_id = -1;
    while (*p) {
        if (p[0] == '$' && (p[1] == '{' || (p[1] >= '0' && p[1] <= '9'))) {
            int id; char def[256]; def[0]=0;
            if (p[1] == '{') {
                const char *q = p + 2; id = 0; while (*q>='0'&&*q<='9'){ id=id*10+(*q-'0'); q++; }
                if (*q == ':') { q++; size_t d=0; while (*q && *q!='}' && d<255) def[d++]=*q++; q++; }
                else if (*q == '}') q++;
                p = q;
            } else { id = p[1]-'0'; p += 2; }
            if (id == 0) { /* final cursor pos, no placeholder */ continue; }
            /* dedupe by id: only the first occurrence of an id is a stop
             * (mirrors $1 later are live-linked to the same placeholder). */
            int seen = 0;
            for (int k = 0; k < e->ts_n; k++) if (e->ts[k].id == id) { seen = 1; break; }
            if (seen) {
                if (def[0]) { ins(buf, out, def, strlen(def)); out += strlen(def); }
                continue;
            }
            if (e->ts_n < SN_MAXTS) {
                e->ts[e->ts_n].id = id; e->ts[e->ts_n].start = out;
                e->ts[e->ts_n].end  = out + strlen(def);
                e->ts_n++;
            }
            if (def[0]) { ins(buf, out, def, strlen(def)); out += strlen(def); }
            last_id = id;
        } else {
            char ch[2] = { *p, 0 }; ins(buf, out, ch, 1); out++; p++;
        }
    }
    (void)del; (void)lenfn; (void)last_id;
    if (e->ts_n > 0) { e->active = 1; e->cur = 0; }
    return e->ts_n;
}

int snippet_next(SnippetEngine *e, size_t *out_from, size_t *out_to) {
    if (!e || !e->active) return 0;
    if (e->cur >= e->ts_n) { e->active = 0; return 0; }
    *out_from = e->ts[e->cur].start;
    *out_to   = e->ts[e->cur].end;
    e->cur++;
    if (e->cur >= e->ts_n) e->active = 0;
    return 1;
}
void snippet_active(SnippetEngine *e, size_t *out_from, size_t *out_to) {
    if (!e || !e->active || e->cur >= e->ts_n) { *out_from = *out_to = 0; return; }
    *out_from = e->ts[e->cur].start;
    *out_to   = e->ts[e->cur].end;
}
int snippet_active_p(const SnippetEngine *e){ return e ? e->active : 0; }
