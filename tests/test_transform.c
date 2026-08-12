/* test_transform.c -- sort + case-transform correctness. */
#include "doc.h"
#include "transform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
static void ck(int c, const char *m){ if(!c){ printf("FAIL: %s\n", m); fails++; } }

static char *result(Doc *d){
    return doc_text(d);
}

int main(void){
    /* sort ascending */
    Doc *d = doc_create("banana\napple\ncherry\n");
    ck(transform_sort_lines(d, SORT_ASC) == 1, "sort asc changed");
    char *t = result(d);
    ck(strcmp(t, "apple\nbanana\ncherry\n") == 0, "sort asc order"); free(t);
    doc_free(d);

    /* sort descending */
    d = doc_create("b\nc\na\n");
    transform_sort_lines(d, SORT_DESC);
    t = result(d);
    ck(strcmp(t, "c\nb\na\n") == 0, "sort desc order"); free(t);
    doc_free(d);

    /* sort case-insensitive */
    d = doc_create("Beta\nalpha\nGAMMA\n");
    transform_sort_lines(d, SORT_ASC_IC);
    t = result(d);
    ck(strcmp(t, "alpha\nBeta\nGAMMA\n") == 0, "sort asc ic"); free(t);
    doc_free(d);

    /* no trailing newline */
    d = doc_create("z\na\nm");
    transform_sort_lines(d, SORT_ASC);
    t = result(d);
    ck(strcmp(t, "a\nm\nz") == 0, "sort no trailing nl"); free(t);
    doc_free(d);

    /* upper */
    d = doc_create("Hello, World!");
    transform_case(d, TRANSFORM_UPPER);
    t = result(d);
    ck(strcmp(t, "HELLO, WORLD!") == 0, "upper"); free(t);
    doc_free(d);

    /* lower */
    d = doc_create("Hello, World!");
    transform_case(d, TRANSFORM_LOWER);
    t = result(d);
    ck(strcmp(t, "hello, world!") == 0, "lower"); free(t);
    doc_free(d);

    /* title */
    d = doc_create("the quick brown fox");
    transform_case(d, TRANSFORM_TITLE);
    t = result(d);
    ck(strcmp(t, "The Quick Brown Fox") == 0, "title"); free(t);
    doc_free(d);

    /* no-op: already sorted returns 0 */
    d = doc_create("a\nb\nc\n");
    ck(transform_sort_lines(d, SORT_ASC) == 0, "already sorted no-op"); 
    doc_free(d);

    /* undo restores */
    d = doc_create("b\na\n");
    transform_sort_lines(d, SORT_ASC);
    doc_undo(d);
    t = result(d);
    ck(strcmp(t, "b\na\n") == 0, "undo sort"); free(t);
    doc_free(d);

    if (fails == 0) printf("TRANSFORM TESTS PASSED\n");
    else printf("TRANSFORM TESTS FAILED (%d)\n", fails);
    return fails ? 1 : 0;
}
