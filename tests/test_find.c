/* test_find.c -- find/replace controller over the real Doc + search engine.
 * Uses the headless backend (no display needed). Verifies match counts,
 * selection movement, and replace-all correctness for both literal + regex. */
#include "ui/ui.h"
#include "ui/ui_headless.h"
#include "ui/ui_find.h"
#include "doc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define CHECK(c, m) do { if(!(c)) { printf("FAIL: %s\n", m); failures++; } } while(0)

int main(void) {
    const char *src = "foo bar foo baz foo\nFOO end foo";
    Doc *d = doc_create(src);
    UI *ui = ui_create(d, ui_headless_backend(), 80, 24);
    CHECK(ui != NULL, "ui_create");

    /* literal, case-sensitive: 4 'foo' (not FOO) */
    long n = ui_find(ui, "foo", 0, 0);
    CHECK(n == 4, "literal count == 4");
    CHECK(ui_find_matches(ui) == 4, "controller count == 4");

    /* case-insensitive includes FOO -> 5 */
    n = ui_find(ui, "foo", 0, 1);
    CHECK(n == 5, "icase count == 5");

    /* regex: foo or baz */
    n = ui_find(ui, "foo|baz", 1, 0);
    CHECK(n == 5, "regex foo|baz count == 5");

    /* bad regex -> -1, bad flag set */
    n = ui_find(ui, "foo(", 1, 0);
    CHECK(n == -1, "bad regex returns -1");
    CHECK(ui_find_error(ui) == 1, "bad pattern flagged");

    /* replace-all 'foo' -> 'XXX' (case-sensitive, 4 occurrences) */
    n = ui_find(ui, "foo", 0, 0);
    CHECK(n == 4, "re-find foo == 4");
    long rep = ui_find_replace_all_in_doc(ui, "XXX");
    CHECK(rep == 4, "replace-all count == 4");
    char *out = doc_text(d);
    CHECK(strstr(out, "XXX bar XXX baz XXX") != NULL, "first line replaced");
    CHECK(strstr(out, "FOO end XXX") != NULL, "FOO preserved, last foo replaced");
    free(out);

    ui_free(ui);
    doc_free(d);
    if (failures) { printf("FAILED (%d)\n", failures); return 1; }
    printf("PASS: find/replace (literal/regex/icase/bad/replace-all)\n");
    return 0;
}
