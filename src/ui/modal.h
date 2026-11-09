#ifndef COCKPIT_UI_MODAL_H
#define COCKPIT_UI_MODAL_H

#include "ui/ui_state.h"

/* The one remaining interrupt: an unrecoverable write (R38). */
void modal_open_error_titled(UiState *ui, const char *title, const char *text);
void modal_draw(const UiState *ui);

#endif
