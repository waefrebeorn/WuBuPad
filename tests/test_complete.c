/* test_complete.c -- word completion + symbol/function list (Phase D, part 2).
 * Completion + symbols tested on the core module directly; ui_complete tested
 * through the headless UI. */
#include "complete.h"
#include "doc.h"
#include "ui/ui.h"
#include "ui/ui_headless.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define CHECK(c, m) do { if(!(c)) { printf("FAIL: %s\n", m); failures++; } } while(0)

/* count how many completions equal `want` */
static int has(char **list, size_t n, const char *want) {
    for (size_t i = 0; i < n; i++) if (strcmp(list[i], want) == 0) return 1;
    return 0;
}

int main(void) {
    const char *src =
        "int foo() { return 0; }\n"
        "int food() { return foo(); }\n"
        "void bar(int foo) { food(); }\n";
    size_t len = strlen(src);

    /* --- completions --- */
    size_t n = 0;
    char **c = doc_complete(src, len, "fo", &n);
    CHECK(n >= 2, "fo completions >= 2");
    CHECK(has(c, n, "foo"), "has foo");
    CHECK(has(c, n, "food"), "has food");
    CHECK(!has(c, n, "fo"), "excludes the prefix itself");
    CHECK(!has(c, n, "bar"), "excludes non-matching");
    doc_complete_free(c, n);

    /* no prefix -> all distinct words */
    c = doc_complete(src, len, "", &n);
    CHECK(n >= 5, "empty prefix -> many words");
    doc_complete_free(c, n);

    /* --- symbols / function list --- */
    size_t ns = 0;
    DocSymbol *s = doc_symbols(src, len, &ns);
    CHECK(ns >= 3, "symbols >= 3");
    int funcs = 0;
    for (size_t i = 0; i < ns; i++) if (s[i].is_func) funcs++;
    CHECK(funcs >= 3, "functions detected (foo/food/bar)");
    doc_symbols_free(s, ns);

    /* --- ui_complete end-to-end (headless) --- */
    Doc *d = doc_create("foo food\nfo");
    UI *ui = ui_create(d, ui_headless_backend(), 80, 24);
    doc_set_cursor(d, 11);            /* end: after "fo" on line 2 */
    ui_complete(ui);                 /* should expand "fo" -> "foo" (first) */
    char *t = doc_text(d);
    CHECK(strcmp(t, "foo food\nfoo") == 0, "ui_complete expands fo -> foo");
    free(t);
    ui_free(ui);
    doc_free(d);

    if (failures) { printf("FAILED (%d)\n", failures); return 1; }
    printf("PASS: completion + symbols (Phase D)\n");
    return 0;
}
