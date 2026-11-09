#include "ui/plugin_flow.h"

#include <stdio.h>

#include "core/metrics.h"
#include "core/plugin_editor.h"

/*
 * Dock policy (KTD3, KTD4, KTD8). Everything here is derived from the editor's
 * cached facts and the metrics sample, so a frame never opens a file or stats
 * the card. The action order is the physical order: safe first, commit last.
 *
 * This file stays free of psp2, vita2d, and ui/modal so the host tests can link
 * it.
 */

#define REBOOT_MIN_PERCENT 20

PluginFlowState plugin_flow_state(void)
{
    /*
     * A write that could not be restored is a repair state, not a draft. The
     * only safe action is UNDO; leaving it as DRAFT advertises SAVE/CONFIRM
     * even though both refuse with REPAIR WITH UNDO FIRST.
     */
    if (g_editor.write_unrecovered)
        return editor_undo_available() ? PLUGIN_FLOW_LAST_WRITE
                                       : PLUGIN_FLOW_DRAFT;

    if (editor_preview_active())
        return PLUGIN_FLOW_PREVIEW;
    if (editor_draft_dirty())
        return PLUGIN_FLOW_DRAFT;

    /*
     * NEXT BOOT is the saved-but-not-booted state. With no pending difference,
     * a differing `.undo` is the contextual last write, not a false SAVED
     * banner: the app cannot tell a relaunch from a reboot (KTD8).
     */
    if (g_editor.pending_count > 0)
        return PLUGIN_FLOW_SAVED;
    if (editor_undo_available())
        return PLUGIN_FLOW_LAST_WRITE;

    return PLUGIN_FLOW_IDLE;
}

static const char *plugin_flow_snapshot_label(EditorSnapshot kind)
{
    switch (kind) {
    case EDITOR_SNAPSHOT_ORIGINAL:
        return "ORIGINAL";
    case EDITOR_SNAPSHOT_SESSION:
        return "PREVIOUS SESSION";
    case EDITOR_SNAPSHOT_UNDO:
        return "UNDO";
    }

    return "SNAPSHOT";
}

const char *plugin_flow_preview_title(void)
{
    static char title[64];

    snprintf(title, sizeof(title), "PREVIEW - %s",
             plugin_flow_snapshot_label(g_editor.preview_kind));
    return title;
}

static const PluginDockAction IDLE_ACTIONS[] = {
    PLUGIN_DOCK_RESTORE_ORIGINAL,
    PLUGIN_DOCK_RESTORE_SESSION,
};

static const PluginDockAction EDIT_ACTIONS[] = {
    PLUGIN_DOCK_DISCARD,
    PLUGIN_DOCK_SAVE,
    PLUGIN_DOCK_CONFIRM,
    PLUGIN_DOCK_RESTORE_ORIGINAL,
    PLUGIN_DOCK_RESTORE_SESSION,
};

static const PluginDockAction PREVIEW_ACTIONS[] = {
    PLUGIN_DOCK_CANCEL,
    PLUGIN_DOCK_SAVE,
    PLUGIN_DOCK_CONFIRM,
};

static const PluginDockAction SAVED_UNDO_ACTIONS[] = {
    PLUGIN_DOCK_UNDO,
    PLUGIN_DOCK_REBOOT,
};

static const PluginDockAction SAVED_REBOOT_ACTIONS[] = {
    PLUGIN_DOCK_REBOOT,
};

static const PluginDockAction LAST_WRITE_ACTIONS[] = {
    PLUGIN_DOCK_UNDO,
};

/*
 * One table for the facts every dock surface reads. SAVED is the documented
 * exception: its action list depends on whether `.undo` still differs.
 */
typedef struct {
    int height;
    const char *title;
    const PluginDockAction *actions;
    int action_count;
    int primary_count; /* 0: every action belongs on the first row */
} FlowSpec;

static const FlowSpec FLOW[PLUGIN_FLOW_COUNT] = {
    [PLUGIN_FLOW_IDLE] = {
        PLUGIN_DOCK_H_IDLE,
        NULL,
        IDLE_ACTIONS,
        (int)(sizeof(IDLE_ACTIONS) / sizeof(IDLE_ACTIONS[0])),
        0,
    },
    [PLUGIN_FLOW_DRAFT] = {
        PLUGIN_DOCK_H_DRAFT,
        "DRAFT - NOT WRITTEN",
        EDIT_ACTIONS,
        (int)(sizeof(EDIT_ACTIONS) / sizeof(EDIT_ACTIONS[0])),
        PLUGIN_DOCK_PRIMARY_ACTIONS,
    },
    [PLUGIN_FLOW_PREVIEW] = {
        PLUGIN_DOCK_H_PREVIEW,
        NULL,
        PREVIEW_ACTIONS,
        (int)(sizeof(PREVIEW_ACTIONS) / sizeof(PREVIEW_ACTIONS[0])),
        0,
    },
    [PLUGIN_FLOW_SAVED] = {
        PLUGIN_DOCK_H_SAVED,
        "SAVED - REBOOT TO APPLY",
        NULL,
        0,
        0,
    },
    [PLUGIN_FLOW_LAST_WRITE] = {
        PLUGIN_DOCK_H_SAVED,
        "LAST WRITE - UNDO AVAILABLE",
        LAST_WRITE_ACTIONS,
        (int)(sizeof(LAST_WRITE_ACTIONS) / sizeof(LAST_WRITE_ACTIONS[0])),
        0,
    },
};

int plugin_flow_dock_height(PluginFlowState state)
{
    if ((unsigned)state >= PLUGIN_FLOW_COUNT)
        return PLUGIN_DOCK_H_IDLE;

    return FLOW[state].height;
}

const char *plugin_flow_state_title(PluginFlowState state)
{
    if ((unsigned)state >= PLUGIN_FLOW_COUNT)
        return NULL;

    return FLOW[state].title;
}

static const PluginDockAction *dock_actions(PluginFlowState state, int *count)
{
    if ((unsigned)state >= PLUGIN_FLOW_COUNT) {
        *count = 0;
        return NULL;
    }

    if (state == PLUGIN_FLOW_SAVED) {
        if (editor_undo_available()) {
            *count = (int)(sizeof(SAVED_UNDO_ACTIONS) /
                           sizeof(SAVED_UNDO_ACTIONS[0]));
            return SAVED_UNDO_ACTIONS;
        }

        *count = (int)(sizeof(SAVED_REBOOT_ACTIONS) /
                       sizeof(SAVED_REBOOT_ACTIONS[0]));
        return SAVED_REBOOT_ACTIONS;
    }

    *count = FLOW[state].action_count;
    return FLOW[state].actions;
}

int plugin_flow_action_count(PluginFlowState state)
{
    int count = 0;

    (void)dock_actions(state, &count);
    return count;
}

PluginDockAction plugin_flow_action_at(PluginFlowState state, int index)
{
    int count = 0;
    const PluginDockAction *actions = dock_actions(state, &count);

    if (actions == NULL || index < 0 || index >= count)
        return PLUGIN_DOCK_NONE;

    return actions[index];
}

int plugin_flow_primary_count(PluginFlowState state)
{
    if ((unsigned)state >= PLUGIN_FLOW_COUNT)
        return 0;

    int count = plugin_flow_action_count(state);
    int primary = FLOW[state].primary_count;

    if (primary > 0 && primary < count)
        return primary;

    return count;
}

const char *plugin_flow_action_label(PluginFlowState state,
                                     PluginDockAction action)
{
    switch (action) {
    case PLUGIN_DOCK_DISCARD:
        return "DISCARD";
    case PLUGIN_DOCK_SAVE:
        return "SAVE, REBOOT LATER";
    case PLUGIN_DOCK_CONFIRM:
        if (state == PLUGIN_FLOW_PREVIEW && editor_preview_discards_draft())
            return "DISCARD DRAFT & RESTORE";
        return "CONFIRM & REBOOT";
    case PLUGIN_DOCK_CANCEL:
        return "CANCEL";
    case PLUGIN_DOCK_UNDO:
        return "UNDO LAST WRITE";
    case PLUGIN_DOCK_REBOOT:
        return "REBOOT NOW";
    case PLUGIN_DOCK_RESTORE_ORIGINAL:
        return "RESTORE ORIGINAL";
    case PLUGIN_DOCK_RESTORE_SESSION:
        return "RESTORE PREVIOUS SESSION";
    case PLUGIN_DOCK_NONE:
    default:
        return "--";
    }
}

int plugin_flow_battery_blocked(void)
{
    if (!g_metrics.power_valid)
        return 1;

    return g_metrics.batt_pct < REBOOT_MIN_PERCENT && !g_metrics.charging;
}

const char *plugin_flow_reboot_block_reason(void)
{
    static char charge_reason[64];

    if (!g_metrics.power_valid)
        return "BATTERY LEVEL UNKNOWN - CHARGE BEFORE REBOOTING";

    if (plugin_flow_battery_blocked()) {
        snprintf(charge_reason, sizeof(charge_reason),
                 "REBOOT BLOCKED - CHARGE TO %d%%", REBOOT_MIN_PERCENT);
        return charge_reason;
    }

    return editor_repair_reason();
}

static int preview_committable(void)
{
    return editor_preview_active() && !editor_preview_no_change() &&
           !editor_preview_locked_removal();
}

int plugin_flow_action_available(PluginFlowState state,
                                 PluginDockAction action)
{
    int battery_blocked = plugin_flow_battery_blocked();

    switch (action) {
    case PLUGIN_DOCK_DISCARD:
        return state == PLUGIN_FLOW_DRAFT && editor_draft_dirty();
    case PLUGIN_DOCK_CANCEL:
        return state == PLUGIN_FLOW_PREVIEW;
    case PLUGIN_DOCK_SAVE:
        if (state == PLUGIN_FLOW_DRAFT)
            return editor_readonly_reason() == NULL;
        if (state == PLUGIN_FLOW_PREVIEW)
            return preview_committable();
        return 0;
    case PLUGIN_DOCK_CONFIRM:
        if (state == PLUGIN_FLOW_DRAFT)
            return !battery_blocked && editor_readonly_reason() == NULL;
        if (state == PLUGIN_FLOW_PREVIEW)
            return preview_committable() && !battery_blocked;
        return 0;
    case PLUGIN_DOCK_UNDO:
        return (state == PLUGIN_FLOW_SAVED ||
                state == PLUGIN_FLOW_LAST_WRITE) &&
               editor_undo_available();
    case PLUGIN_DOCK_REBOOT:
        return state == PLUGIN_FLOW_SAVED &&
               plugin_flow_reboot_block_reason() == NULL;
    case PLUGIN_DOCK_RESTORE_ORIGINAL:
        return (state == PLUGIN_FLOW_IDLE || state == PLUGIN_FLOW_DRAFT) &&
               editor_can_restore() &&
               editor_snapshot_available(EDITOR_SNAPSHOT_ORIGINAL);
    case PLUGIN_DOCK_RESTORE_SESSION:
        return (state == PLUGIN_FLOW_IDLE || state == PLUGIN_FLOW_DRAFT) &&
               editor_can_restore() &&
               editor_snapshot_available(EDITOR_SNAPSHOT_SESSION);
    case PLUGIN_DOCK_NONE:
    default:
        return 0;
    }
}

PluginDockAction plugin_flow_primary_action(PluginFlowState state)
{
    if (state == PLUGIN_FLOW_DRAFT || state == PLUGIN_FLOW_PREVIEW) {
        if (plugin_flow_action_available(state, PLUGIN_DOCK_CONFIRM))
            return PLUGIN_DOCK_CONFIRM;
        if (plugin_flow_action_available(state, PLUGIN_DOCK_SAVE))
            return PLUGIN_DOCK_SAVE;
        return PLUGIN_DOCK_NONE;
    }

    if (state == PLUGIN_FLOW_SAVED) {
        return plugin_flow_action_available(state, PLUGIN_DOCK_REBOOT)
                   ? PLUGIN_DOCK_REBOOT
                   : PLUGIN_DOCK_NONE;
    }

    if (state == PLUGIN_FLOW_LAST_WRITE) {
        return plugin_flow_action_available(state, PLUGIN_DOCK_UNDO)
                   ? PLUGIN_DOCK_UNDO
                   : PLUGIN_DOCK_NONE;
    }

    return PLUGIN_DOCK_NONE;
}

static const char *batch_warning_text(void)
{
    return editor_batch_warning();
}

void plugin_flow_dock_warning(const UiState *ui, char *buf, size_t cap)
{
    if (buf == NULL || cap == 0)
        return;

    buf[0] = '\0';

    if (ui != NULL && ui->dock_message[0] != '\0') {
        snprintf(buf, cap, "%s", ui->dock_message);
        return;
    }

    if (editor_preview_active()) {
        const char *locked = editor_preview_locked_path();

        if (locked != NULL) {
            snprintf(buf, cap, "RESTORE DROPS %s - NOT ALLOWED", locked);
            return;
        }

        if (editor_preview_no_change()) {
            snprintf(buf, cap, "NO CHANGES - NOTHING TO RESTORE");
            return;
        }

        if (editor_preview_discards_draft()) {
            snprintf(buf, cap, "DRAFT WILL BE DISCARDED");
            return;
        }
    }

    PluginFlowState state = plugin_flow_state();

    if (state == PLUGIN_FLOW_DRAFT || state == PLUGIN_FLOW_PREVIEW ||
        state == PLUGIN_FLOW_SAVED) {
        const char *block = plugin_flow_reboot_block_reason();

        if (block != NULL) {
            snprintf(buf, cap, "%s", block);
            return;
        }
    }

    if (state == PLUGIN_FLOW_DRAFT || state == PLUGIN_FLOW_PREVIEW) {
        const char *risk = batch_warning_text();

        if (risk != NULL) {
            snprintf(buf, cap, "%s", risk);
            return;
        }

        /* The save-only warning is about the focused action, not the state. */
        if (ui != NULL && ui->focus == UI_FOCUS_DOCK &&
            plugin_flow_action_at(state, ui->dock_choice) == PLUGIN_DOCK_SAVE)
            snprintf(buf, cap, "%s", plugin_flow_save_only_text());
    }
}

PluginFlowCommand plugin_flow_focused_command(const UiState *ui)
{
    if (ui == NULL || ui->focus != UI_FOCUS_DOCK)
        return PLUGIN_COMMAND_NONE;

    PluginFlowState state = plugin_flow_state();
    PluginDockAction action = plugin_flow_action_at(state, ui->dock_choice);

    if (!plugin_flow_action_available(state, action))
        return PLUGIN_COMMAND_NONE;

    switch (action) {
    case PLUGIN_DOCK_DISCARD:
        return PLUGIN_COMMAND_DISCARD;
    case PLUGIN_DOCK_CANCEL:
        return PLUGIN_COMMAND_CANCEL;
    case PLUGIN_DOCK_SAVE:
        return PLUGIN_COMMAND_SAVE;
    case PLUGIN_DOCK_CONFIRM:
        return PLUGIN_COMMAND_CONFIRM;
    case PLUGIN_DOCK_UNDO:
        return PLUGIN_COMMAND_PREVIEW_UNDO;
    case PLUGIN_DOCK_REBOOT:
        return PLUGIN_COMMAND_REBOOT;
    case PLUGIN_DOCK_RESTORE_ORIGINAL:
        return PLUGIN_COMMAND_PREVIEW_ORIGINAL;
    case PLUGIN_DOCK_RESTORE_SESSION:
        return PLUGIN_COMMAND_PREVIEW_SESSION;
    case PLUGIN_DOCK_NONE:
    default:
        return PLUGIN_COMMAND_NONE;
    }
}

const char *plugin_flow_save_only_text(void)
{
    return "SAVE ONLY - NOTHING CHANGES UNTIL THE VITA RESTARTS / THE PLUGINS "
           "RUNNING NOW STAY LOADED.";
}

/* The first action the user can actually take, or -1 when the dock is empty. */
static int safe_choice(PluginFlowState state)
{
    int count = plugin_flow_action_count(state);

    for (int i = 0; i < count; ++i) {
        if (plugin_flow_action_available(state,
                                         plugin_flow_action_at(state, i)))
            return i;
    }

    return -1;
}

void plugin_flow_focus_list(UiState *ui)
{
    if (ui != NULL)
        ui->focus = UI_FOCUS_LIST;
}

void plugin_flow_dock_enter(UiState *ui)
{
    if (ui == NULL)
        return;

    PluginFlowState state = plugin_flow_state();
    int choice = safe_choice(state);

    if (choice < 0) {
        ui->focus = UI_FOCUS_LIST;
        return;
    }

    ui->focus = UI_FOCUS_DOCK;
    ui->dock_choice = choice;
}

void plugin_flow_dock_toggle(UiState *ui)
{
    if (ui == NULL)
        return;

    if (ui->focus == UI_FOCUS_DOCK)
        ui->focus = UI_FOCUS_LIST;
    else
        plugin_flow_dock_enter(ui);
}

void plugin_flow_dock_reset(UiState *ui)
{
    if (ui == NULL)
        return;

    PluginFlowState state = plugin_flow_state();
    int choice = safe_choice(state);

    if (choice < 0) {
        ui->focus = UI_FOCUS_LIST;
        ui->dock_choice = 0;
        return;
    }

    ui->dock_choice = choice;
}

/*
 * Availability is derived every frame, but the cursor is not. When the
 * focused action becomes unavailable (battery gate, undo removed, state
 * change), move focus to the safe action instead of leaving Cross inert.
 */
void plugin_flow_revalidate_focus(UiState *ui)
{
    if (ui == NULL || ui->focus != UI_FOCUS_DOCK)
        return;

    if (plugin_flow_focused_command(ui) == PLUGIN_COMMAND_NONE)
        plugin_flow_dock_reset(ui);
}

/*
 * A failed commit keeps the action that failed focused, so the next Cross
 * retries it instead of jumping to DISCARD. The index is clamped only when the
 * state shrank; an unavailable in-range action stays dim until the user moves.
 */
void plugin_flow_dock_keep(UiState *ui)
{
    if (ui == NULL || ui->focus != UI_FOCUS_DOCK)
        return;

    PluginFlowState state = plugin_flow_state();
    int count = plugin_flow_action_count(state);

    if (ui->dock_choice >= 0 && ui->dock_choice < count)
        return;

    int choice = safe_choice(state);

    if (choice < 0) {
        ui->focus = UI_FOCUS_LIST;
        ui->dock_choice = 0;
        return;
    }

    ui->dock_choice = choice;
}

void plugin_flow_dock_move(UiState *ui, int direction)
{
    if (ui == NULL || ui->focus != UI_FOCUS_DOCK || direction == 0)
        return;

    PluginFlowState state = plugin_flow_state();
    int count = plugin_flow_action_count(state);
    int current = ui->dock_choice;

    /*
     * A state change can leave the cursor on an action the new state does not
     * offer. Fall back to the safe action before moving, so Left/Right never
     * starts from a stale chip.
     */
    if (current < 0 || current >= count ||
        !plugin_flow_action_available(state,
                                      plugin_flow_action_at(state, current))) {
        current = safe_choice(state);

        if (current < 0) {
            ui->focus = UI_FOCUS_LIST;
            return;
        }

        ui->dock_choice = current;
    }

    for (int i = current + direction; i >= 0 && i < count; i += direction) {
        if (plugin_flow_action_available(state,
                                         plugin_flow_action_at(state, i))) {
            ui->dock_choice = i;
            return;
        }
    }
}
