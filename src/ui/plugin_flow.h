#ifndef COCKPIT_UI_PLUGIN_FLOW_H
#define COCKPIT_UI_PLUGIN_FLOW_H

#include <stddef.h>

#include "core/plugin_editor.h"
#include "ui/ui_state.h"

/*
 * PLUGINS dock policy: state, action order, labels, availability, focus, and
 * the inline copy.
 *
 * Everything here is derived from the editor's cached facts and the metrics
 * sample, so the dock never opens a file or stats the card. It lives outside
 * app.c so the host tests and the preview can exercise the same rules (D26,
 * KTD3, KTD7), and it stays free of psp2, vita2d, and ui/modal so the host
 * tests can link it.
 */

/*
 * The derived dock state and its action list (KTD3, KTD8). This is policy, not
 * drawing: the panel reads the state, the labels, and availability, while the
 * app asks for the focused command. No psp2 or vita2d header is allowed here,
 * so the host tests can link the module.
 */
typedef enum {
    PLUGIN_FLOW_IDLE = 0,
    PLUGIN_FLOW_DRAFT,
    PLUGIN_FLOW_PREVIEW,
    PLUGIN_FLOW_SAVED,
    PLUGIN_FLOW_LAST_WRITE,
    PLUGIN_FLOW_COUNT,
} PluginFlowState;

typedef enum {
    PLUGIN_DOCK_NONE = -1,
    PLUGIN_DOCK_DISCARD = 0,
    PLUGIN_DOCK_SAVE,
    PLUGIN_DOCK_CONFIRM,
    PLUGIN_DOCK_CANCEL,
    PLUGIN_DOCK_UNDO,
    PLUGIN_DOCK_REBOOT,
    PLUGIN_DOCK_RESTORE_ORIGINAL,
    PLUGIN_DOCK_RESTORE_SESSION,
} PluginDockAction;

#define PLUGIN_DOCK_H_IDLE 64
#define PLUGIN_DOCK_H_DRAFT 159
#define PLUGIN_DOCK_H_PREVIEW 123
#define PLUGIN_DOCK_H_SAVED 111

/* The draft dock's first row holds DISCARD, SAVE, and CONFIRM. */
#define PLUGIN_DOCK_PRIMARY_ACTIONS 3

/* The one truth table from KTD8, derived from the editor and cached facts. */
PluginFlowState plugin_flow_state(void);

/* Pinned strip height in pixels, one value per dock state. */
int plugin_flow_dock_height(PluginFlowState state);

/* The state line, or NULL for IDLE which carries no title. */
const char *plugin_flow_state_title(PluginFlowState state);

/* `PREVIEW - ORIGINAL`, `PREVIEW - PREVIOUS SESSION`, or `PREVIEW - UNDO`. */
const char *plugin_flow_preview_title(void);

/* The horizontal action list. The safe action is first, the commit last. */
int plugin_flow_action_count(PluginFlowState state);

/* How many leading actions belong on the first row; the draft dock puts the
 * two restore actions on a second row. */
int plugin_flow_primary_count(PluginFlowState state);

PluginDockAction plugin_flow_action_at(PluginFlowState state, int index);
const char *plugin_flow_action_label(PluginFlowState state,
                                     PluginDockAction action);

/* Unavailable actions stay in place, dimmed, so the row never reflows. */
int plugin_flow_action_available(PluginFlowState state,
                                 PluginDockAction action);

/* The rightmost available commit, or NONE. `SAVE` becomes primary when the
 * battery blocks the reboot (R34). */
PluginDockAction plugin_flow_primary_action(PluginFlowState state);

/* 1 when the reboot action is gated by the battery read (R34, R37). */
int plugin_flow_battery_blocked(void);

/*
 * The full reboot gate: invalid power, below 20% while discharging, an
 * unrecovered write, or a failed recovery. Returns the reason string, or NULL
 * when a reboot may proceed. ABOUT and the dock read this same function so
 * the two surfaces cannot diverge.
 */
const char *plugin_flow_reboot_block_reason(void);

/*
 * The one inline warning line. Precedence: the app's transient message, the
 * preview's locked/no-change/discard reason, the shared reboot gate (battery,
 * then repair state), a kernel or
 * storage warning for a changed row, then the `SAVE, REBOOT LATER` copy while
 * that action is focused. Empty when the dock has nothing to say.
 */
void plugin_flow_dock_warning(const UiState *ui, char *buf, size_t cap);

/* The exact R35 copy for `SAVE, REBOOT LATER`. */
const char *plugin_flow_save_only_text(void);

/*
 * What the focused dock action does, with no SDK call attached. app.c maps the
 * command to the editor and platform; the host tests exercise the mapping
 * without a console (KTD7).
 */
typedef enum {
    PLUGIN_COMMAND_NONE = 0,
    PLUGIN_COMMAND_DISCARD,
    PLUGIN_COMMAND_CANCEL,
    PLUGIN_COMMAND_SAVE,
    PLUGIN_COMMAND_CONFIRM,
    PLUGIN_COMMAND_PREVIEW_ORIGINAL,
    PLUGIN_COMMAND_PREVIEW_SESSION,
    PLUGIN_COMMAND_PREVIEW_UNDO,
    PLUGIN_COMMAND_REBOOT,
} PluginFlowCommand;

/* NONE unless the dock is focused on an available action. */
PluginFlowCommand plugin_flow_focused_command(const UiState *ui);

/*
 * Dock focus (KTD4, R28-R29). UiState carries only the cursor; these functions
 * own the transitions. Down from the list calls dock_enter, which focuses the
 * safe action (DISCARD, CANCEL, UNDO, or the first available) and leaves the
 * focus on the list when no action is available. Up/Circle calls focus_list.
 * Left/Right calls dock_move, which skips unavailable actions and clamps.
 * A state change calls dock_reset before the next frame.
 */
void plugin_flow_focus_list(UiState *ui);
void plugin_flow_dock_enter(UiState *ui);
void plugin_flow_dock_toggle(UiState *ui);
void plugin_flow_dock_reset(UiState *ui);
void plugin_flow_revalidate_focus(UiState *ui);
void plugin_flow_dock_keep(UiState *ui);
void plugin_flow_dock_move(UiState *ui, int direction);

#endif
