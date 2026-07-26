/* ui_macro.c -- record + replay (see ui_macro.h). */
#include "ui_macro.h"
#include "ui.h"

#include <stdlib.h>
#include <string.h>

typedef struct { char ch; int key; } Ev;

struct UIMacro {
    Ev   *ev;
    size_t n, cap;
    int recording;
};

UIMacro *ui_macro_create(void) {
    UIMacro *m = calloc(1, sizeof *m);
    return m;
}
void ui_macro_free(UIMacro *m) {
    if (!m) return;
    free(m->ev);
    free(m);
}
void ui_macro_record_start(UIMacro *m) {
    if (!m) return;
    ui_macro_clear(m);
    m->recording = 1;
}
void ui_macro_record_stop(UIMacro *m) {
    if (!m) return;
    m->recording = 0;
}
int ui_macro_recording(const UIMacro *m) { return m && m->recording; }

void ui_macro_clear(UIMacro *m) {
    if (!m) return;
    m->n = 0;
}
void ui_macro_add(UIMacro *m, char ch, int key) {
    if (!m || !m->recording) return;
    /* ignore pure render/no-op keys in the recording */
    if (key == UI_KEY_NONE && ch == 0) return;
    if (m->n + 1 >= m->cap) {
        m->cap = m->cap ? m->cap * 2 : 64;
        m->ev = realloc(m->ev, m->cap * sizeof(Ev));
    }
    m->ev[m->n].ch = ch;
    m->ev[m->n].key = key;
    m->n++;
}
size_t ui_macro_len(const UIMacro *m) { return m ? m->n : 0; }

void ui_macro_replay(struct UI *ui, const UIMacro *m) {
    if (!ui || !m) return;
    for (size_t i = 0; i < m->n; i++)
        ui_apply(ui, m->ev[i].ch, m->ev[i].key);
}
