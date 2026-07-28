/* ui_atom.h -- Atom subsystem API (declarations only). See ui_atom.c. */
#ifndef WUBUPAD_UI_ATOM_H
#define WUBUPAD_UI_ATOM_H

#include "ui.h"   /* UI, command types */

/* Build the command registry + palette + package manager for a UI. */
void ui_atom_init(UI *ui);
/* Tear them down. */
void ui_atom_free(UI *ui);

/* Open the command palette. */
void ui_open_palette(UI *ui);
/* Feed one keystroke to the palette while open. Returns 1 if consumed. */
int  ui_palette_feed(UI *ui, char ch, int key);

#endif /* WUBUPAD_UI_ATOM_H */
