#ifndef COCKPIT_UI_MODAL_H
#define COCKPIT_UI_MODAL_H

#include "ui/launch_card.h"
#include "ui/ui_state.h"

/* The unrecoverable-write interrupt (R38). */
void modal_open_error_titled(UiState *ui, const char *title, const char *text);

/* The launch safety gate: Cross or Circle acknowledges it. */
void modal_open_safety(UiState *ui, const LaunchCard *card);

/* The reboot choice: Cross confirms, Circle cancels. */
void modal_open_reboot(UiState *ui, const char *title, const char *text);

void modal_draw(const UiState *ui);

#endif
