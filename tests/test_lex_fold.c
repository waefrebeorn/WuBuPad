/* test_lex_fold.c -- verify lex_folds + lex_symbols on sample C. */
#include "lex.h"
#include <stdio.h>
#include <string.h>

static const char *SAMPLE =
    "int main(void) {\n"
    "    int x = 0;\n"
    "    for (int i = 0; i < 10; i++) {\n"
    "        x += i;\n"
    "    }\n"
    "    return x;\n"
    "}\n"
    "\n"
    "void helper(int a, int b) {\n"
    "    printf(\"%d\\n\", a + b);\n"
    "}\n";

int main(void) {
    size_t len = strlen(SAMPLE);
    int bad = 0;

    LexFold folds[64];
    size_t nf = lex_folds(SAMPLE, len, folds, 64);
    printf("[fold] regions=%zu\n", nf);
    if (nf < 3) { fprintf(stderr, "[fold] expected >=3 folds, got %zu\n", nf); bad++; }
    else {
        int found_main = 0, found_helper = 0;
        for (size_t k = 0; k < nf; k++) {
            if (folds[k].start == 0 && folds[k].end >= 6) found_main = 1;
            if (folds[k].start == 8 && folds[k].end == 10) found_helper = 1;
        }
        if (!found_main) { fprintf(stderr, "[fold] main block (0-6) missing\n"); bad++; }
        if (!found_helper) { fprintf(stderr, "[fold] helper block (8-10) missing\n"); bad++; }
    }

    LexSym syms[64];
    size_t ns = lex_symbols(SAMPLE, len, syms, 64);
    printf("[sym] symbols=%zu\n", ns);
    if (ns < 2) { fprintf(stderr, "[sym] expected >=2 symbols, got %zu\n", ns); bad++; }
    else {
        /* first symbol should be 'main' at line 0 */
        char name[64];
        memcpy(name, SAMPLE + syms[0].name_off, syms[0].name_len); name[syms[0].name_len] = 0;
        if (strcmp(name, "main") != 0) { fprintf(stderr, "[sym] first symbol '%s' not 'main'\n", name); bad++; }
        if (syms[0].line != 0) { fprintf(stderr, "[sym] main line %zu not 0\n", syms[0].line); bad++; }
        /* second symbol 'helper' at line 8 */
        memcpy(name, SAMPLE + syms[1].name_off, syms[1].name_len); name[syms[1].name_len] = 0;
        if (strcmp(name, "helper") != 0) { fprintf(stderr, "[sym] second symbol '%s' not 'helper'\n", name); bad++; }
        if (syms[1].line != 8) { fprintf(stderr, "[sym] helper line %zu not 8\n", syms[1].line); bad++; }
    }

    printf("done bad=%d\n", bad);
    return bad ? 1 : 0;
}
