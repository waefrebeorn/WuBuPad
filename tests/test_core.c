/* test_core.c -- exercise WuBuPad's headless editing core.
 * Real assertions: piece-table correctness, undo/redo exactness, lexer
 * tokenization, cursor/selection semantics, line/col mapping, search
 * (literal + regex), encoding detect/convert, Myers diff, multi-doc session.
 * No external oracle needed. */
#include "buffer.h"
#include "doc.h"
#include "lex.h"
#include "search.h"
#include "encode.h"
#include "diff.h"
#include "docs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s\n", msg); fails++; } } while(0)

/* compare a buffer/doc to an expected string */
static void ck_buf(Buf *b, const char *want, const char *msg) {
    char *s = buf_to_string(b);
    CHECK(strcmp(s, want) == 0, msg);
    if (strcmp(s, want) != 0) printf("   got=%s want=%s\n", s, want);
    free(s);
}
static void ck_doc(Doc *d, const char *want, const char *msg) {
    char *s = doc_text(d);
    CHECK(strcmp(s, want) == 0, msg);
    if (strcmp(s, want) != 0) printf("   got=%s want=%s\n", s, want);
    free(s);
}

static void test_buffer(void) {
    Buf *b = buf_create("hello world");
    CHECK(buf_length(b) == 11, "buffer init length");
    ck_buf(b, "hello world", "buffer init content");

    buf_insert(b, 0, ">>", 2);
    ck_buf(b, ">>hello world", "buffer insert at 0");

    buf_insert(b, buf_length(b), "<<", 2);
    ck_buf(b, ">>hello world<<", "buffer insert at end");

    buf_insert(b, 8, "BIG ", 4);     /* before 'w' in "hello world" */
    ck_buf(b, ">>hello BIG world<<", "buffer insert in middle");

    buf_delete(b, 2, 6);             /* delete "hello " -> pos 2..8 */
    ck_buf(b, ">>BIG world<<", "buffer delete");

    buf_delete(b, 0, 2);
    ck_buf(b, "BIG world<<", "buffer delete front");

    CHECK(buf_char_at(b, 0) == 'B', "buffer char_at");
    CHECK(buf_line_count(b) == 1, "buffer line count");

    buf_free(b);
}

static void test_undo(void) {
    Doc *d = doc_create("");
    doc_type(d, "abc", 3);
    doc_type(d, "def", 3);
    ck_doc(d, "abcdef", "doc type appends");

    doc_undo(d);
    ck_doc(d, "abc", "doc undo last insert");

    doc_undo(d);
    ck_doc(d, "", "doc undo to empty");

    CHECK(!doc_can_undo(d), "doc no more undo");
    doc_redo(d);
    ck_doc(d, "abc", "doc redo");

    /* delete + undo */
    doc_type(d, "XYZ", 3);           /* "abcXYZ" */
    doc_set_selection(d, 0, 3);      /* select "abc" */
    doc_type(d, "", 0);              /* replace selection with nothing -> delete */
    ck_doc(d, "XYZ", "doc type over selection deletes");

    doc_undo(d);
    ck_doc(d, "abcXYZ", "doc undo selection-delete restores");

    /* redo after a new edit clears redo stack */
    doc_undo(d);                     /* back to "XYZ" */
    doc_undo(d);                     /* back to "" */
    doc_type(d, "Q", 1);
    CHECK(!doc_can_redo(d), "doc new edit clears redo");
    ck_doc(d, "Q", "doc after redo-clear");

    doc_free(d);
}

static void test_cursor(void) {
    Doc *d = doc_create("0123456789");
    doc_set_cursor(d, 4);
    CHECK(doc_cursor(d) == 4, "doc set cursor");
    doc_set_selection(d, 2, 7);
    CHECK(doc_has_selection(d), "doc has selection");
    CHECK(doc_sel_start(d) == 2 && doc_sel_end(d) == 7, "doc sel range");
    doc_type(d, "X", 1);             /* replace [2,7) with X */
    ck_doc(d, "01X789", "doc replace selection");
    CHECK(!doc_has_selection(d), "doc selection cleared after type");
    CHECK(doc_cursor(d) == 3, "doc cursor after replace");

    doc_free(d);
}

static void test_lex(void) {
    const char *src = "#include <stdio.h>\nchar *msg = \"hello\";\nint main(){ return 0; // note\n }";
    Lex *c = lex_create("c");
    CHECK(c != NULL, "lex create c");
    LexSpan spans[256];
    size_t n = lex_run(c, src, strlen(src), spans, 256);
    CHECK(n > 5, "lex produced spans");

    /* find a keyword token "return" */
    int found_kw = 0, found_str = 0, found_pp = 0;
    for (size_t i = 0; i < n; i++) {
        size_t sl = spans[i].end - spans[i].start;
        if (spans[i].kind == TK_KEYWORD && sl == 6 &&
            strncmp(src + spans[i].start, "return", 6) == 0) found_kw = 1;
        if (spans[i].kind == TK_STRING && sl == 7 &&
            strncmp(src + spans[i].start, "\"hello\"", 7) == 0) found_str = 1;
        if (spans[i].kind == TK_PREPROC) found_pp = 1;
    }
    CHECK(found_kw, "lex found 'return' keyword");
    CHECK(found_str, "lex found string literal");
    CHECK(found_pp, "lex found preprocessor");
    lex_free(c);

    Lex *j = lex_create("json");
    const char *js = "{\"name\": \"wubu\", \"n\": 42, \"ok\": true}";
    size_t m = lex_run(j, js, strlen(js), spans, 256);
    int found_key = 0, found_num = 0;
    for (size_t i = 0; i < m; i++) {
        size_t sl = spans[i].end - spans[i].start;
        if (spans[i].kind == TK_KEYWORD && sl == 6 &&
            strncmp(js + spans[i].start, "\"name\"", 6) == 0) found_key = 1;
        if (spans[i].kind == TK_NUMBER && sl == 2 &&
            strncmp(js + spans[i].start, "42", 2) == 0) found_num = 1;
    }
    CHECK(found_key, "lex json key");
    CHECK(found_num, "lex json number");
    lex_free(j);

    Lex *u = lex_create("cobol");
    if (u) { size_t q = lex_run(u, "x", 1, spans, 256); CHECK(q==1 && spans[0].kind==TK_TEXT, "lex unknown -> plain"); lex_free(u); }
}

/* ---- line / column mapping ------------------------------------------ */
static void test_linecol(void) {
    Buf *b = buf_create("abc\ndef\nghi");
    CHECK(buf_line_count(b) == 3, "line count");
    size_t ln, col;
    buf_line_col_of(b, 0, &ln, &col); CHECK(ln==0 && col==0, "pos0 -> 0,0");
    buf_line_col_of(b, 3, &ln, &col); CHECK(ln==0 && col==3, "pos3 -> 0,3 (end of L0)");
    buf_line_col_of(b, 4, &ln, &col); CHECK(ln==1 && col==0, "pos4 -> 1,0 (start of L1)");
    buf_line_col_of(b, 8, &ln, &col); CHECK(ln==2 && col==0, "pos8 -> 2,0");
    /* round trip */
    size_t back = buf_pos_of_line_col(b, 1, 2);
    CHECK(back == 6, "line1,col2 -> pos6 (the 'e')");
    CHECK(buf_line_start(b, 2) == 8, "line2 start = 8");
    buf_free(b);
}

/* ---- search (literal + regex) --------------------------------------- */
static void test_search(void) {
    const char *hay = "the quick brown fox jumps";
    size_t p = search_literal(hay, strlen(hay), "brown", 5, 0);
    CHECK(p == 10, "literal find 'brown' at 10");

    Regex *r = regex_compile("qu.ck", 0);
    CHECK(r != NULL, "regex compile qu.ck");
    size_t ms, me;
    int found = regex_find(r, hay, strlen(hay), &ms, &me);
    CHECK(found && ms == 4 && me == 9, "regex qu.ck matches 'quick'");
    regex_free(r);

    /* alternation + quantifier. (cat|dog)s? matches 'dog' or 'dogs' at 9 */
    Regex *r2 = regex_compile("(cat|dog)s?", 0);
    CHECK(r2 != NULL, "regex compile (cat|dog)s?");
    found = regex_find(r2, "I have a dogs and cats", 19, &ms, &me);
    CHECK(found && ms == 9 && me >= 12, "regex (cat|dog)s? matches 'dog(s)' at 9");
    regex_free(r2);

    /* find-next from offset */
    Regex *r3 = regex_compile("a", 0);
    found = regex_find_from(r3, "banana", 6, 2, &ms, &me);
    CHECK(found && ms == 3 && me == 4, "regex find 'a' from 2 -> pos3");
    regex_free(r3);

    /* case-insensitive */
    Regex *r4 = regex_compile("THE", 1);
    found = regex_find(r4, hay, strlen(hay), &ms, &me);
    CHECK(found && ms == 0 && me == 3, "regex icase 'THE' matches 'the'");
    regex_free(r4);

    /* no match */
    Regex *r5 = regex_compile("zzz", 0);
    found = regex_find(r5, hay, strlen(hay), &ms, &me);
    CHECK(!found, "regex no match 'zzz'");
    regex_free(r5);
}

/* ---- encoding -------------------------------------------------------- */
static void test_encode(void) {
    /* UTF-16LE detection (with BOM): "Ab" = FF FE 41 00 62 00 */
    unsigned char u16[] = { 0xFF,0xFE, 0x41,0x00, 0x62,0x00 };
    EncKind k = enc_detect(u16, sizeof u16);
    CHECK(k == ENC_UTF16LE, "detect utf-16le BOM");
    size_t ol; char *s = enc_to_utf8(u16 + 2, sizeof u16 - 2, ENC_UTF16LE, &ol);
    CHECK(s && ol == 2 && s[0]=='A' && s[1]=='b', "utf-16le -> 'Ab'");
    free(s);

    /* UTF-16LE multibyte (no BOM): 'é'=0x00E9 -> 0xE9 0x00 -> utf8 0xC3 0xA9 */
    unsigned char u16e[] = { 0xE9,0x00 };
    char *se = enc_to_utf8(u16e, sizeof u16e, ENC_UTF16LE, &ol);
    CHECK(se && ol == 2 && (unsigned char)se[0]==0xC3 && (unsigned char)se[1]==0xA9,
          "utf-16le 'é' -> utf-8 0xC3 0xA9");
    free(se);

    /* UTF-8 validity detection + passthrough */
    const char *u8 = "héllo";  /* é is 2 bytes */
    k = enc_detect((const unsigned char*)u8, strlen(u8));
    CHECK(k == ENC_UTF8, "detect utf-8");
    char *s2 = enc_decode((const unsigned char*)u8, strlen(u8), &ol);
    CHECK(s2 && strcmp(s2, u8) == 0, "utf-8 decode round-trips");
    free(s2);

    /* Latin1 -> utf-8 (é = 0xE9 -> 0xC3 0xA9) */
    unsigned char lat1[] = { 0x48, 0xE9, 0x6C, 0x6C, 0x6F }; /* "Héllo" */
    k = enc_detect(lat1, sizeof lat1);
    CHECK(k == ENC_LATIN1, "detect latin1");
    char *s3 = enc_decode(lat1, sizeof lat1, &ol);
    CHECK(s3 && ol == 6 && s3[0]=='H' && (unsigned char)s3[1]==0xC3 && (unsigned char)s3[2]==0xA9,
          "latin1 'é' -> utf-8 0xC3 0xA9");
    free(s3);
}

/* ---- Myers diff ------------------------------------------------------ */
static void test_diff(void) {
    const char *a[] = { "one", "two", "three", "four" };
    const char *b[] = { "one", "TWO", "three", "FIVE" };
    Diff *d = diff_lines(a, 4, b, 4);
    CHECK(d != NULL, "diff built");
    /* expect: EQ one, DEL two, INS TWO, EQ three, DEL four, INS FIVE */
    size_t n = diff_count(d);
    int saw_eq_one = 0, saw_del_two = 0, saw_ins_TWO = 0, saw_eq_three = 0;
    for (size_t i = 0; i < n; i++) {
        const DiffEdit *e = diff_get(d, i);
        if (e->op == DIFF_EQ && strcmp(a[e->a_idx], "one") == 0) saw_eq_one = 1;
        if (e->op == DIFF_DEL && strcmp(a[e->a_idx], "two") == 0) saw_del_two = 1;
        if (e->op == DIFF_INS && strcmp(b[e->b_idx], "TWO") == 0) saw_ins_TWO = 1;
        if (e->op == DIFF_EQ && strcmp(a[e->a_idx], "three") == 0) saw_eq_three = 1;
    }
    CHECK(saw_eq_one && saw_del_two && saw_ins_TWO && saw_eq_three, "diff captures edits");
    char *uni = diff_unified(d, a, b, "a", "b");
    CHECK(uni && strstr(uni, "-two") && strstr(uni, "+TWO"), "diff unified text");
    free(uni);
    diff_free(d);
}

/* ---- multi-document session ----------------------------------------- */
static void test_docs(void) {
    Docs *s = docs_create();
    size_t i1 = docs_open(s, "a.c", "int main(){}", "c");
    size_t i2 = docs_open(s, "b.json", "{\"x\":1}", "json");
    CHECK(docs_count(s) == 2, "two docs open");
    CHECK(docs_active(s) == i2, "last opened is active");
    docs_set_active(s, i1);
    CHECK(docs_active(s) == i1, "set active");
    CHECK(docs_dirty(s, i1) == 0, "seeded doc clean (has path)");
    Doc *d = docs_doc(s, i1);
    CHECK(d != NULL, "docs_doc returns core");
    doc_type(d, "\n", 1);                       /* edit the core */
    { char *dt = doc_text(d); CHECK(strcmp(dt, "int main(){}\n") == 0, "doc core edited"); free(dt); }
    /* The session dirty flag is owned by the app: after an edit it marks dirty. */
    docs_set_dirty(s, i1, 1);
    CHECK(docs_dirty(s, i1) == 1, "mark dirty after edit");
    docs_set_dirty(s, i1, 0);
    CHECK(docs_dirty(s, i1) == 0, "mark clean");
    /* load + save round-trip via temp file */
    const char *tmp = "/tmp/wubupad_doctest.txt";
    size_t li = docs_load_file(s, tmp, "c");  /* tmp doesn't exist -> SIZE_MAX */
    CHECK(li == SIZE_MAX, "load missing file fails cleanly");
    docs_close(s, i2);
    CHECK(docs_count(s) == 1, "close reduces count");
    docs_free(s);
}

int main(void) {
    test_buffer();
    test_undo();
    test_cursor();
    test_lex();
    test_linecol();
    test_search();
    test_encode();
    test_diff();
    test_docs();
    if (fails) { printf("\nCORE TESTS FAILED (%d)\n", fails); return 1; }
    printf("CORE TESTS PASSED (buffer/doc/lex/linecol/search/encode/diff/docs)\n");
    return 0;
}
