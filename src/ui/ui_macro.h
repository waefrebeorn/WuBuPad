/* ui_macro.h -- record + replay a sequence of editor input events.
 *
 * A macro is an ordered list of (char, UI_KEY_*) events. While recording, the
 * UI feeds every key it would apply into the macro. Replaying re-feeds them
 * through the same apply path, so a recorded editing session reproduces
 * exactly. Backend-agnostic. Clean C11. */
#ifndef WUBUPAD_UI_MACRO_H
#define WUBUPAD_UI_MACRO_H

#include <stddef.h>

typedef struct UIMacro UIMacro;

UIMacro *ui_macro_create(void);
void      ui_macro_free(UIMacro *m);

void ui_macro_record_start(UIMacro *m);
void ui_macro_record_stop(UIMacro *m);
int  ui_macro_recording(const UIMacro *m);

/* Append an event (only effective while recording). */
void ui_macro_add(UIMacro *m, char ch, int key);
/* Drop any recorded events. */
void ui_macro_clear(UIMacro *m);
size_t ui_macro_len(const UIMacro *m);

/* Re-feed every recorded event through `ui_step(ui, ch, key)`. */
struct UI;
void ui_macro_replay(struct UI *ui, const UIMacro *m);

#endif /* WUBUPAD_UI_MACRO_H */
