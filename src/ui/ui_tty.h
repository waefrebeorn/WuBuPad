/* ui_tty.h -- curses-free ANSI terminal UI backend. */
#ifndef WUBUPAD_UI_TTY_H
#define WUBUPAD_UI_TTY_H

#include "ui.h"

/* The terminal backend vtable (for ui_create). */
const UI_Backend *ui_tty_backend(void);

/* Put the terminal into raw mode (no line buffering / echo) for interactive
 * editing. Returns -1 if `ui` is not on the TTY backend or tcsetattr fails. */
int ui_tty_enable_raw(UI *ui);

#endif /* WUBUPAD_UI_TTY_H */
