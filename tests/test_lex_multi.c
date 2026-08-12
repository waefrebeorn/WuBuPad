/* test_lex_multi.c -- verify the config-driven lexers (py/js/css/sql/md). */
#include "lex.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ printf("FAIL: %s\n", m); fails++; } }while(0)

static int has_kind(const char *lang, const char *src, LexTok want){
    Lex *l = lex_create(lang);
    if (!l) return 0;
    LexSpan sp[128];
    size_t n = lex_run(l, src, strlen(src), sp, 128);
    lex_free(l);
    for (size_t i=0;i<n;i++) if (sp[i].kind == want) return 1;
    return 0;
}

int main(void){
    CK(has_kind("py", "def foo():\n    # comment\n    return 42\n", TK_KEYWORD), "python def keyword");
    CK(has_kind("py", "x = \"hello\"  # str", TK_STRING), "python string");
    CK(has_kind("py", "class Foo:", TK_KEYWORD), "python class keyword");
    CK(has_kind("py", "import os", TK_KEYWORD), "python import");

    CK(has_kind("js", "function add(a,b){ return a+b; }", TK_KEYWORD), "js function");
    CK(has_kind("js", "const x = 42; // note", TK_KEYWORD), "js const");
    CK(has_kind("js", "let y = null;", TK_TYPE), "js null type");
    CK(has_kind("js", "/* block */ var z = true;", TK_COMMENT), "js block comment");

    CK(has_kind("css", "body { color: red; }", TK_TYPE), "css color");
    CK(has_kind("css", "p { display: flex; }", TK_TYPE), "css flex");

    CK(has_kind("sql", "SELECT * FROM users WHERE id = 1;", TK_KEYWORD), "sql select");
    CK(has_kind("sql", "-- comment\nSELECT name", TK_COMMENT), "sql line comment");

    CK(has_kind("md", "# Heading\nbody text", TK_TYPE), "md heading");
    CK(has_kind("md", "- bullet item", TK_TYPE), "md bullet");

    if (fails == 0) printf("MULTI-LEXER TESTS PASSED\n");
    else printf("MULTI-LEXER TESTS FAILED (%d)\n", fails);
    return fails ? 1 : 0;
}
