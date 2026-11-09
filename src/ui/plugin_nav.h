#ifndef COCKPIT_UI_PLUGIN_NAV_H
#define COCKPIT_UI_PLUGIN_NAV_H

#include "core/input.h"
#include "ui/plugin_flow.h"
#include "ui/ui_state.h"

/*
 * The PLUGINS input policy, separate from the frame loop.
 *
 * app.c owns the pad and the clock; this module owns the choice of what an
 * input event means for the tab, the list, the dock, and the modal. Keeping it
 * platform-free lets the host tests exercise the routing that otherwise only
 * runs on the console (KTD4, KTD7).
 */

#define PLUGIN_NAV_VITALS_STEP 60.0f

typedef enum {
    PLUGIN_NAV_NONE = 0,
    PLUGIN_NAV_TAB_LEFT,
    PLUGIN_NAV_TAB_RIGHT,
    PLUGIN_NAV_MODAL_DISMISS,
    PLUGIN_NAV_MODAL_CONFIRM,
    PLUGIN_NAV_MODAL_CANCEL,
    PLUGIN_NAV_VITALS_SCROLL_UP,
    PLUGIN_NAV_VITALS_SCROLL_DOWN,
    PLUGIN_NAV_SELECT_UP,
    PLUGIN_NAV_SELECT_DOWN,
    PLUGIN_NAV_DOCK_ENTER,
    PLUGIN_NAV_DOCK_LEAVE,
    PLUGIN_NAV_DOCK_TOGGLE,
    PLUGIN_NAV_DOCK_LEFT,
    PLUGIN_NAV_DOCK_RIGHT,
    PLUGIN_NAV_ACTIVATE_DOCK,
    PLUGIN_NAV_ACTIVATE_ROW,
    PLUGIN_NAV_ACTIVATE_ABOUT,
} PluginNavAction;

/*
 * What the event means in the current UI state. `row_count` is the editor's
 * row count and `preview_active` is the editor's preview flag, passed in so the
 * function reads no global state and the host test can drive every case.
 */
PluginNavAction plugin_nav_action(const UiState *ui, InputEvent event,
                                  int row_count, int preview_active);

typedef enum {
    PLUGIN_COMMIT_NONE = 0,
    PLUGIN_COMMIT_SAVE,
    PLUGIN_COMMIT_REBOOT,
} PluginCommitMode;

/* SAVE commits without a reboot; CONFIRM commits and requests one. */
PluginCommitMode plugin_nav_commit_mode(PluginFlowCommand command);

/* The next tab in `direction` (-1 left, +1 right), wrapping at either end. */
UiTab plugin_nav_tab_step(UiTab tab, int direction);

/* Clamp a scroll offset to [0, max]. */
void plugin_nav_scroll_vitals(UiState *ui, float delta, int max);
void plugin_nav_scroll_plugins(UiState *ui, float delta, int max);

/* Set an absolute plugin scroll offset, clamped by the same policy. */
void plugin_nav_set_plugins_scroll(UiState *ui, float target, int max);

/* The per-frame analog scroll for the active surface. */
void plugin_nav_apply_scroll(UiState *ui, float scroll_y, float dt,
                             int vitals_max, int plugins_max);

/* The VITALS D-pad step; VITALS has no selection. */
void plugin_nav_vitals_step(UiState *ui, int direction, int max);

#endif
