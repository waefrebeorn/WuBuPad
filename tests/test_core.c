/* test_core.c -- exercise WuBuPad's headless editing core.
 * Real assertions: piece-table correctness, undo/redo exactness, lexer
 * tokenization, cursor/selection semantics. No external oracle needed. */
#include "buffer.h"
#include "doc.h"
#include "lex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(void) {
    test_buffer();
    test_undo();
    test_cursor();
    test_lex();
    if (fails) { printf("\nCORE TESTS FAILED (%d)\n", fails); return 1; }
    printf("CORE TESTS PASSED (buffer/doc/lex: insert/delete/undo/redo/cursor/selection/lexer)\n");
    return 0;
}
