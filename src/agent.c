/* agent.c -- wubuOS protocol dispatcher over the verified headless core.
 * Self-contained C11; reuses Docs/Doc/search/encode/diff/lex. No GUI. */
#include "agent.h"
#include "docs.h"
#include "doc.h"
#include "buffer.h"
#include "search.h"
#include "encode.h"
#include "diff.h"
#include "lex.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct Agent { Docs *sess; };

Agent *agent_create(void) { Agent *a = malloc(sizeof *a); if(!a) return NULL; a->sess = docs_create(); return a; }
void agent_free(Agent *a) { if(!a) return; docs_free(a->sess); free(a); }

static char *err(const char *msg) {
    JVal *o = j_obj(); j_obj_put(o, "error", j_str(msg));
    char *s = j_emit(o); j_free(o); return s;
}

static JVal *doc_summary(Agent *a, size_t id) {
    Doc *d = docs_doc(a->sess, id);
    char *text = doc_text(d);
    size_t bytes = strlen(text);
    JVal *o = j_obj();
    j_obj_put(o, "id", j_num((double)id));
    j_obj_put(o, "path", j_str(docs_path(a->sess, id) ? docs_path(a->sess, id) : ""));
    j_obj_put(o, "lang", j_str(docs_lang(a->sess, id) ? docs_lang(a->sess, id) : ""));
    j_obj_put(o, "dirty", j_bool(docs_dirty(a->sess, id)));
    j_obj_put(o, "lines", j_num((double)doc_lines(d)));
    j_obj_put(o, "bytes", j_num((double)bytes));
    free(text);
    return o;
}

char *agent_handle(Agent *a, const char *command_json) {
    if (!a || !command_json) return err("null");
    const char *end = NULL;
    JVal *cmd = j_parse(command_json, &end);
    if (!cmd || j_type(cmd) != J_OBJ) { j_free(cmd); return err("bad json"); }
    const JVal *c = j_obj_get(cmd, "cmd");
    if (!c || j_type(c) != J_STR) { j_free(cmd); return err("missing cmd"); }
    const char *name = j_as_str(c);

    JVal *res = NULL;

    if (strcmp(name, "open") == 0 || strcmp(name, "ingest") == 0) {
        const JVal *text = j_obj_get(cmd, "text");
        const JVal *path = j_obj_get(cmd, "path");
        const JVal *lang = j_obj_get(cmd, "lang");
        if (!text || j_type(text) != J_STR) { j_free(cmd); return err("open: text required"); }
        const char *p = path && j_type(path)==J_STR ? j_as_str(path) : NULL;
        const char *l = lang && j_type(lang)==J_STR ? j_as_str(lang) : "txt";
        size_t id = docs_open(a->sess, p, j_as_str(text), l);
        res = doc_summary(a, id);
    }
    else if (strcmp(name, "close") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        if (!idv) { j_free(cmd); return err("close: id required"); }
        int ok = docs_close(a->sess, (size_t)j_as_num(idv));
        res = j_obj(); j_obj_put(res, "ok", j_bool(ok));
    }
    else if (strcmp(name, "list") == 0) {
        JVal *arr = j_arr();
        for (size_t i = 0; i < docs_count(a->sess); i++) j_arr_push(arr, doc_summary(a, i));
        res = j_obj(); j_obj_put(res, "docs", arr);
    }
    else if (strcmp(name, "get") == 0 || strcmp(name, "regurgitate") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        if (!idv) { j_free(cmd); return err("get: id required"); }
        size_t id = (size_t)j_as_num(idv);
        Doc *d = docs_doc(a->sess, id);
        if (!d) { j_free(cmd); return err("get: no such id"); }
        char *text = doc_text(d);
        if (strcmp(name, "regurgitate") == 0) {
            const JVal *from = j_obj_get(cmd, "from");
            const JVal *to = j_obj_get(cmd, "to");
            size_t ln = doc_lines(d);
            size_t f = from ? (size_t)j_as_num(from) : 0;
            size_t t = to ? (size_t)j_as_num(to) : ln;
            if (f > t) f = t;
            if (t > ln) t = ln;
            /* exact byte range [start_f, start_t) preserves original content,
             * including trailing newlines, with no invented bytes. */
            size_t start = doc_line_byte_start(d, f);
            size_t endpos = doc_line_byte_start(d, t);
            if (endpos < start) endpos = start;
            size_t seglen = endpos - start;
            char *seg = malloc(seglen + 1);
            memcpy(seg, text + start, seglen);
            seg[seglen] = '\0';
            JVal *out = j_obj();
            j_obj_put(out, "id", j_num((double)id));
            j_obj_put(out, "text", j_str(seg));
            free(seg);
            res = out;
        } else {
            res = j_obj();
            j_obj_put(res, "id", j_num((double)id));
            j_obj_put(res, "text", j_str(text));
            j_obj_put(res, "lines", j_num((double)doc_lines(d)));
            j_obj_put(res, "bytes", j_num((double)strlen(text)));
        }
        free(text);
    }
    else if (strcmp(name, "edit") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        const JVal *pos = j_obj_get(cmd, "pos");
        const JVal *ins = j_obj_get(cmd, "insert");
        if (!idv || !pos || !ins || j_type(ins) != J_STR) { j_free(cmd); return err("edit: id,pos,insert required"); }
        Doc *d = docs_doc(a->sess, (size_t)j_as_num(idv));
        if (!d) { j_free(cmd); return err("edit: no such id"); }
        doc_insert(d, (size_t)j_as_num(pos), j_as_str(ins), strlen(j_as_str(ins)));
        docs_set_dirty(a->sess, (size_t)j_as_num(idv), 1);
        char *t = doc_text(d); size_t b = strlen(t); free(t);
        res = j_obj(); j_obj_put(res, "ok", j_bool(1)); j_obj_put(res, "bytes", j_num((double)b));
    }
    else if (strcmp(name, "replace") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        const JVal *from = j_obj_get(cmd, "from");
        const JVal *to = j_obj_get(cmd, "to");
        const JVal *tx = j_obj_get(cmd, "text");
        if (!idv || !from || !to || !tx || j_type(tx) != J_STR) { j_free(cmd); return err("replace: id,from,to,text required"); }
        Doc *d = docs_doc(a->sess, (size_t)j_as_num(idv));
        if (!d) { j_free(cmd); return err("replace: no such id"); }
        doc_replace(d, (size_t)j_as_num(from), (size_t)j_as_num(to), j_as_str(tx));
        docs_set_dirty(a->sess, (size_t)j_as_num(idv), 1);
        res = j_obj(); j_obj_put(res, "ok", j_bool(1));
    }
    else if (strcmp(name, "search") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        const JVal *pat = j_obj_get(cmd, "pattern");
        const JVal *rx = j_obj_get(cmd, "regex");
        const JVal *ic = j_obj_get(cmd, "icase");
        if (!idv || !pat || j_type(pat) != J_STR) { j_free(cmd); return err("search: id,pattern required"); }
        Doc *d = docs_doc(a->sess, (size_t)j_as_num(idv));
        if (!d) { j_free(cmd); return err("search: no such id"); }
        char *text = doc_text(d);
        JVal *arr = j_arr();
        int regex = rx && j_as_bool(rx);
        int icase = ic && j_as_bool(ic);
        if (regex) {
            Regex *r = regex_compile(j_as_str(pat), icase);
            if (!r) { free(text); j_free(cmd); return err("search: bad pattern"); }
            size_t ms, me, from = 0;
            while (regex_find_from(r, text, strlen(text), from, &ms, &me)) {
                JVal *m = j_obj();
                j_obj_put(m, "start", j_num((double)ms));
                j_obj_put(m, "end", j_num((double)me));
                size_t ln = 0, col = 0; doc_line_col(d, ms, &ln, &col);
                j_obj_put(m, "line", j_num((double)ln));
                j_arr_push(arr, m);
                if (me <= from) from++; else from = me;
                if (from >= strlen(text)) break;
            }
            regex_free(r);
        } else {
            const char *needle = j_as_str(pat);
            size_t nlen = strlen(needle), from = 0, hlen = strlen(text);
            for (;;) {
                size_t p = search_literal(text, hlen, needle, nlen, from);
                if (p == (size_t)-1) break;
                JVal *m = j_obj();
                j_obj_put(m, "start", j_num((double)p));
                j_obj_put(m, "end", j_num((double)(p + nlen)));
                size_t ln = 0, col = 0; doc_line_col(d, p, &ln, &col);
                j_obj_put(m, "line", j_num((double)ln));
                j_arr_push(arr, m);
                from = p + nlen; if (from >= hlen) break;
            }
        }
        free(text);
        res = j_obj(); j_obj_put(res, "matches", arr);
    }
    else if (strcmp(name, "lines") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        if (!idv) { j_free(cmd); return err("lines: id required"); }
        Doc *d = docs_doc(a->sess, (size_t)j_as_num(idv));
        if (!d) { j_free(cmd); return err("lines: no such id"); }
        res = j_obj(); j_obj_put(res, "lines", j_num((double)doc_lines(d)));
    }
    else if (strcmp(name, "save") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        if (!idv) { j_free(cmd); return err("save: id required"); }
        size_t id = (size_t)j_as_num(idv);
        int ok = docs_save_file(a->sess, id);
        res = j_obj(); j_obj_put(res, "ok", j_bool(ok == 0));
        j_obj_put(res, "path", j_str(docs_path(a->sess, id) ? docs_path(a->sess, id) : ""));
    }
    else if (strcmp(name, "encode") == 0) {
        const JVal *tx = j_obj_get(cmd, "text");
        if (!tx || j_type(tx) != J_STR) { j_free(cmd); return err("encode: text required"); }
        const char *t = j_as_str(tx);
        size_t ol; char *u = enc_decode((const unsigned char*)t, strlen(t), &ol);
        res = j_obj(); j_obj_put(res, "utf8", j_str(u ? u : ""));
        free(u);
    }
    else if (strcmp(name, "diff") == 0) {
        const JVal *av = j_obj_get(cmd, "a");
        const JVal *bv = j_obj_get(cmd, "b");
        if (!av || !bv || j_type(av) != J_ARR || j_type(bv) != J_ARR) { j_free(cmd); return err("diff: a,b arrays required"); }
        size_t na = j_len(av), nb = j_len(bv);
        const char **la = malloc((na?na:1)*sizeof *la), **lb = malloc((nb?nb:1)*sizeof *lb);
        for (size_t i = 0; i < na; i++) { const JVal *e = j_arr_at(av, i); la[i] = (e && j_type(e)==J_STR) ? j_as_str(e) : ""; }
        for (size_t i = 0; i < nb; i++) { const JVal *e = j_arr_at(bv, i); lb[i] = (e && j_type(e)==J_STR) ? j_as_str(e) : ""; }
        Diff *df = diff_lines(la, na, lb, nb);
        JVal *arr = j_arr();
        if (df) {
            for (size_t i = 0; i < diff_count(df); i++) {
                const DiffEdit *e = diff_get(df, i);
                JVal *o = j_obj();
                const char *op = e->op == DIFF_EQ ? "eq" : e->op == DIFF_DEL ? "del" : "ins";
                j_obj_put(o, "op", j_str(op));
                j_obj_put(o, "a", j_num((double)e->a_idx));
                j_obj_put(o, "b", j_num((double)e->b_idx));
                j_arr_push(arr, o);
            }
            diff_free(df);
        }
        free(la); free(lb);
        res = j_obj(); j_obj_put(res, "edit", arr);
    }
    else if (strcmp(name, "lex") == 0) {
        const JVal *idv = j_obj_get(cmd, "id");
        if (!idv) { j_free(cmd); return err("lex: id required"); }
        Doc *d = docs_doc(a->sess, (size_t)j_as_num(idv));
        if (!d) { j_free(cmd); return err("lex: no such id"); }
        const char *lang = docs_lang(a->sess, (size_t)j_as_num(idv));
        Lex *lx = lex_create(lang && *lang ? lang : "c");
        char *text = doc_text(d);
        LexSpan spans[512]; size_t n = lex_run(lx, text, strlen(text), spans, 512);
        JVal *arr = j_arr();
        for (size_t i = 0; i < n; i++) {
            JVal *o = j_obj();
            j_obj_put(o, "kind", j_num((double)spans[i].kind));
            j_obj_put(o, "start", j_num((double)spans[i].start));
            j_obj_put(o, "end", j_num((double)spans[i].end));
            size_t len = spans[i].end - spans[i].start;
            char *seg = malloc(len + 1); memcpy(seg, text + spans[i].start, len); seg[len] = '\0';
            j_obj_put(o, "text", j_str(seg)); free(seg);
            j_arr_push(arr, o);
        }
        free(text); lex_free(lx);
        res = j_obj(); j_obj_put(res, "tokens", arr);
    }
    else {
        j_free(cmd);
        return err("unknown command");
    }

    j_free(cmd);
    char *out = j_emit(res);
    j_free(res);
    return out;
}

int agent_serve(Agent *a, void *in, void *out) {
    FILE *fin = in, *fout = out;
    char *line = NULL; size_t cap = 0; ssize_t len;
    while ((len = getline(&line, &cap, fin)) != -1) {
        /* trim trailing newline */
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;
        char *result = agent_handle(a, line);
        if (!result) break;
        fprintf(fout, "%s\n", result);
        fflush(fout);
        free(result);
        /* quit? */
        JVal *c = j_parse(line, NULL);
        if (c) {
            const JVal *cmd = j_obj_get(c, "cmd");
            int quit = (cmd && strcmp(j_as_str(cmd), "quit") == 0);
            j_free(c);
            if (quit) break;
        }
    }
    free(line);
    return 0;
}
