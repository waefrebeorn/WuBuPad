/* ui_headless.h -- headless (null/recording) UI backend.
 * Exposes the backend vtable and the test/recording accessors. Opaque to the
 * core; only tests and the recorder use the queue/line helpers. Clean C11. */
#ifndef WUBUPAD_UI_HEADLESS_H
#define WUBUPAD_UI_HEADLESS_H

#include "ui.h"

/* The headless backend vtable (for ui_create). */
const UI_Backend *ui_headless_backend(void);

/* Append a scripted key to a headless-bound UI's input queue.
 * Returns -1 if `ui` is not on the headless backend. */
int  ui_headless_queue_key(UI *ui, const char *ch, int key);

/* Read back a recorded viewport line (row) for assertions. Returned pointer
 * is backend-owned; do not free. Returns "" if out of range. */
const char *ui_headless_line(UI *ui, int row);

/* internal: backend-state accessors used by ui.c (which owns the UI type).
 * Not for external callers. */
int  ui__hl_queue(void *bstate, const char *ch, int key);
const char *ui__hl_line(void *bstate, int row);

#endif /* WUBUPAD_UI_HEADLESS_H */
