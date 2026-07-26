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
#include "ui/ui.h"
#include "ui/ui_tty.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = 0;
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    int agent_mode = 1;
    const char *edit_file = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--agent") == 0)      agent_mode = 1;
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

    /* Interactive TUI mode */
    char *text = edit_file ? slurp(edit_file) : NULL;
    Doc *d = doc_create(text ? text : "");
    free(text);
    UI *ui = ui_create(d, ui_tty_backend(), 0, 0);
    if (!ui) { fprintf(stderr, "ui_create failed\n"); doc_free(d); return 1; }
    if (ui_tty_enable_raw(ui) != 0) {
        fprintf(stderr, "failed to enter raw mode (not a tty?)\n");
        ui_free(ui); doc_free(d); return 1;
    }
    printf("\x1b[2J\x1b[HWuBuPad -- ctrl-z undo, ctrl-q quit. Editing %s\n",
           edit_file ? edit_file : "<new>");
    ui_run(ui, edit_file ? "c" : NULL);
    /* restore screen */
    ui_tty_enable_raw(ui);  /* no-op; destroy handles restore */
    ui_free(ui);
    doc_free(d);
    return 0;
}
