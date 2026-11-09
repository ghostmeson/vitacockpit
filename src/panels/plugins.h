#ifndef COCKPIT_PANELS_PLUGINS_H
#define COCKPIT_PANELS_PLUGINS_H

#include "ui/ui_state.h"

/* One row: name, description, and the gap to the next. Shared with app.c's
 * selection scroll and the panel's own geometry. */
#define PLUGINS_ROW_STRIDE 52.0f
#define PLUGINS_SELECT_MARGIN 20.0f

/* Height of the PLUGINS canvas, including the orphan section. */
float plugins_content_height(void);

/* Canvas y of a row, used to keep the selection inside the viewport. */
int plugins_row_y(int row);

/* Largest valid scroll offset for the PLUGINS canvas. */
int plugins_max_scroll(void);

/* Visible list height below the header and above the pinned dock. */
int plugins_viewport_height(void);

/* Screen y where the list viewport ends and the dock begins. */
float plugins_list_bottom(void);

/*
 * Height of the pinned transaction dock for the current derived state (see
 * PLUGIN_DOCK_H_*). The state block gets 12 px of air above and below it. The
 * list viewport and the scroll range shrink by this amount, so no row is ever
 * hidden behind the dock.
 */
float plugins_dock_height(void);

/* Draw the full-screen editable ledger and the pinned dock. */
void plugins_screen_draw(const UiState *ui, float content_h);

#endif
