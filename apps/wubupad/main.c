/* wubupad.c -- the WuBuPad entry point.
 *
 * Two modes:
 *   1. NDJSON agent mode (default, when stdin is not a TTY or `--agent`):
 *      wubuOS drives WuBuPad over stdin/stdout as a document ingestion +
 *      regurgitation engine (the machine "AGI GUI").
 *   2. Interactive TUI mode (`--edit [file]` or when stdin is a TTY): a real,
 *      usable terminal editor backed by the same headless core via ui_tty.
 *
 * Both reuse the exact same core; only the front-end differs. Clean C11. */
#include "agent.h"
#include "doc.h"
#include "docs.h"
#include "ui/ui.h"
#include "ui/ui_tty.h"
#include "ui/ui_gfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int agent_mode = 1;
    const char *edit_file = NULL;
    int gfx_mode = 0;
    int want_light = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--agent") == 0)      agent_mode = 1;
        else if (strcmp(argv[i], "--gfx") == 0)   gfx_mode = 1;
        else if (strcmp(argv[i], "--theme") == 0 && i + 1 < argc) {
            want_light = (strcmp(argv[++i], "light") == 0);
        }
        else if (strcmp(argv[i], "--edit") == 0) { agent_mode = 0; if (i+1 < argc) edit_file = argv[++i]; }
        else if (argv[i][0] != '-') {             agent_mode = 0; edit_file = argv[i]; }
    }
    /* a real terminal with no file and no --agent flag -> interactive */
    if (!agent_mode && isatty(STDIN_FILENO)) {
        /* fall through to TUI below */
    } else if (!isatty(STDIN_FILENO) && agent_mode) {
        Agent *a = agent_create();
        if (!a) { fprintf(stderr, "{\"error\":\"oom\"}\n"); return 1; }
        int rc = agent_serve(a, stdin, stdout);
        agent_free(a);
        return rc ? 1 : 0;
    }

    /* Interactive mode (TUI or GFX) -- open the file into a Docs session so
     * the tab strip works; reuse the editing core. */
    Docs *docs = docs_create();
    const char *lang = edit_file ? "c" : NULL;
    if (edit_file) {
        size_t idx = docs_load_file(docs, edit_file, lang);
        if (idx == (size_t)-1) {
            fprintf(stderr, "cannot open %s\n", edit_file);
            docs_free(docs); return 1;
        }
    } else {
        docs_open(docs, NULL, "", lang);
    }
    Doc *d = docs_doc(docs, docs_active(docs));

    const UI_Backend *be = gfx_mode ? ui_gfx_backend() : ui_tty_backend();
    UI *ui = ui_create(d, be, 0, 0);
    if (!ui) { fprintf(stderr, "ui_create failed\n"); docs_free(docs); return 1; }
    ui_set_docs(ui, docs);
    ui_set_theme(ui, want_light ? 0 : 1);

    if (gfx_mode) {
        ui_run(ui, lang);
        ui_free(ui);
        docs_free(docs);
        return 0;
    }

    if (ui_tty_enable_raw(ui) != 0) {
        fprintf(stderr, "failed to enter raw mode (not a tty?)\n");
        ui_free(ui); docs_free(docs); return 1;
    }
    printf("\x1b[2J\x1b[HWuBuPad -- ctrl-z undo, ctrl-q quit, ctrl-t next tab, ctrl-f find. %s\n",
           edit_file ? edit_file : "<new>");
    ui_run(ui, lang);
    ui_free(ui);
    docs_free(docs);
    return 0;
}