#include "app/app.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
#include <string.h>
#include <vita2d.h>

#include "app/screen.h"
#include "core/input.h"
#include "core/metrics.h"
#include "core/platform.h"
#include "core/plugin_editor.h"
#include "core/plugins.h"
#include "gfx/layout.h"
#include "panels/plugins.h"
#include "panels/registry.h"
#include "ui/launch_card.h"
#include "ui/modal.h"
#include "ui/plugin_flow.h"
#include "ui/plugin_nav.h"
#include "ui/ui_state.h"

/*
 * Lifecycle, time, scheduling, and input. This file owns the frame loop and
 * the one mutable UiState. What is drawn lives in screen.c; how anything looks
 * lives in gfx/theme; the dock copy lives in ui/plugin_flow.
 */

#define MICROS_PER_SECOND 1000000.0

/* Analog axes arrive as 0..255 with 128 at rest. */
#define STICK_AXIS_CENTER 128.0f
#define STICK_AXIS_RANGE 127.0f

/* Short enough for the dock and the 96-byte editor error strings. */
#define ERR_TEXT_MAX 96

/* Set once at startup: if analog sampling cannot be enabled, keep both sticks
 * centred instead of treating raw zero as full up-travel. */
static int s_analog_ready = 1;

static void set_dock_message(UiState *ui, const char *text);
static void request_reboot(UiState *ui);

static float clamp_float(float value, float low, float high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static float stick_axis(int raw)
{
    return clamp_float(((float)raw - STICK_AXIS_CENTER) / STICK_AXIS_RANGE,
                       -1.0f, 1.0f);
}

static int first_row(void)
{
    return g_editor.row_count > 0 ? 0 : -1;
}

static void keep_selection_valid(UiState *ui)
{
    if (ui->selection < 0 || ui->selection >= g_editor.row_count)
        ui->selection = first_row();
}

static void keep_selection_visible(UiState *ui)
{
    if (ui->selection < 0 || ui->selection >= g_editor.row_count)
        return;

    int y = plugins_row_y(ui->selection);
    int top = (int)ui->plugins_scroll;
    int visible = plugins_viewport_height();
    float target = ui->plugins_scroll;

    if (y < top + (int)PLUGINS_SELECT_MARGIN)
        target = (float)(y - (int)PLUGINS_SELECT_MARGIN);
    else if (y + (int)PLUGINS_SELECT_MARGIN + (int)PLUGINS_ROW_STRIDE >
             top + visible)
        target = (float)(y + (int)PLUGINS_SELECT_MARGIN +
                         (int)PLUGINS_ROW_STRIDE - visible);

    plugin_nav_set_plugins_scroll(ui, target, plugins_max_scroll());
}

static void move_selection(UiState *ui, int direction)
{
    if (g_editor.row_count == 0) {
        ui->selection = -1;
        return;
    }

    int index = ui->selection;
    if (index < 0 || index >= g_editor.row_count)
        index = 0;

    index += direction;
    if (index < 0)
        index = 0;
    if (index >= g_editor.row_count)
        index = g_editor.row_count - 1;

    ui->selection = index;
    keep_selection_visible(ui);
}

static int selection_visible(const UiState *ui)
{
    if (ui->selection < 0 || ui->selection >= g_editor.row_count)
        return 0;

    int y = plugins_row_y(ui->selection);
    int screen_y =
        VIEW_Y + VIEW_TOP_GAP - (int)ui->plugins_scroll + y;
    int list_bottom = (int)plugins_list_bottom();

    return screen_y + (int)PLUGINS_ROW_STRIDE > VIEW_Y &&
           screen_y < list_bottom;
}

static void set_dock_message(UiState *ui, const char *text)
{
    snprintf(ui->dock_message, sizeof(ui->dock_message), "%s",
             text == NULL ? "" : text);
}

static void clear_dock_message(UiState *ui)
{
    ui->dock_message[0] = '\0';
}

static void set_about_message(UiState *ui, const char *text)
{
    snprintf(ui->about_message, sizeof(ui->about_message), "%s",
             text == NULL ? "" : text);
}

static void clear_about_message(UiState *ui)
{
    ui->about_message[0] = '\0';
}

/*
 * Cross on a list row stages it. Nothing is written and no dialog opens: a
 * change is intent until the dock commits it (KD1, R22). A row the editor will
 * not switch leaves the badge or the header exception to say why.
 */
static void stage_selected_row(UiState *ui)
{
    if (ui->selection < 0 || ui->selection >= g_editor.row_count)
        return;

    if (editor_preview_active())
        return;

    /*
     * Scrolling past the last row reveals the orphan section. Cross must not
     * act on a row whose control is no longer on screen; bring it back first.
     */
    if (!selection_visible(ui)) {
        keep_selection_visible(ui);
        return;
    }

    EditorAction mode = editor_next_action(ui->selection);

    if (mode == EDITOR_ACTION_NONE)
        return;

    char err[ERR_TEXT_MAX];

    if (editor_stage_toggle(ui->selection, mode, err, (int)sizeof(err)) !=
        EDITOR_WRITE_OK) {
        set_dock_message(ui, err);
        plugin_flow_dock_reset(ui);
        return;
    }

    clear_dock_message(ui);

    /* The draft dock is taller, so the viewport just shrank under the row. */
    keep_selection_visible(ui);
    plugin_flow_dock_reset(ui);
}

static void close_modal(UiState *ui)
{
    ui->modal = UI_MODAL_NONE;
}

static void request_reboot(UiState *ui)
{
    if (platform_request_reboot() < 0) {
        modal_open_error_titled(
            ui, "RESTART FAILED",
            "THE SYSTEM REFUSED THE RESTART REQUEST. HOLD THE POWER BUTTON "
            "FOR 5 SECONDS, THEN POWER ON.");
        set_dock_message(ui, "RESTART FAILED. TURN OFF AND ON.");
        return;
    }

    /*
     * A 0 return means the request was accepted, not that the reset happened.
     * If this frame still runs, the modal says what to do instead of the screen
     * silently promising a restart (D70).
     */
    modal_open_error_titled(
        ui, "REBOOT REQUESTED",
        "IF THE SCREEN STAYS ON, HOLD THE POWER BUTTON FOR 5 SECONDS, THEN "
        "POWER ON.");
    set_dock_message(ui, "REBOOT REQUESTED. IF IT STAYS ON, HOLD POWER 5 SEC.");
}

/*
 * One commit path for SAVE and CONFIRM. A preview commits the snapshot it is
 * showing; a draft commits the working set. The editor owns the stale check,
 * the `.undo`, and the write (KTD5).
 */
/*
 * Cross on ABOUT. The gate runs first, so a blocked reboot never opens a
 * confirmation that would refuse itself. When the gate allows it, the body
 * states what a reboot will do to the current editor state: a draft or preview
 * is lost, a saved batch is applied, and an idle config is untouched.
 */
static void activate_about(UiState *ui)
{
    const char *block = plugin_flow_reboot_block_reason();

    if (block != NULL) {
        set_about_message(ui, block);
        return;
    }

    clear_about_message(ui);

    const char *title = "REBOOT";
    const char *body = "REBOOT THE CONSOLE?";

    if (editor_preview_active()) {
        title = "REBOOT AND DISCARD";

        if (editor_preview_discards_draft())
            body = "THE RESTORE PREVIEW AND STAGED DRAFT WILL BE DISCARDED. "
                   "APPLY THE RESTORE WITH DISCARD DRAFT & RESTORE IN "
                   "PLUGINS. REBOOT WITHOUT APPLYING?";
        else
            body = "THE RESTORE PREVIEW WILL BE DISCARDED. APPLY IT WITH "
                   "CONFIRM & REBOOT IN PLUGINS. REBOOT WITHOUT APPLYING?";
    } else if (editor_draft_dirty()) {
        title = "REBOOT AND DISCARD";
        body = "STAGED PLUGIN CHANGES WILL BE DISCARDED. APPLY THEM WITH "
               "CONFIRM & REBOOT IN PLUGINS. REBOOT WITHOUT APPLYING?";
    } else if (g_editor.pending_count > 0) {
        body = "THE SAVED CONFIG WILL BE APPLIED AT NEXT BOOT. REBOOT NOW?";
    }

    modal_open_reboot(ui, title, body);
}

static void do_commit(UiState *ui, int reboot)
{
    char err[ERR_TEXT_MAX];
    char selected_path[PLUGIN_PATH_MAX];
    int had_selection = ui->selection >= 0 &&
                        ui->selection < g_editor.row_count;

    if (had_selection)
        snprintf(selected_path, sizeof(selected_path), "%s",
                 g_editor.rows[ui->selection].path);

    EditorWriteResult result = editor_preview_active()
                                  ? editor_commit_preview(err, (int)sizeof(err))
                                  : editor_commit_draft(err, (int)sizeof(err));

    if (result != EDITOR_WRITE_OK) {
        set_dock_message(ui, err);
        plugin_flow_dock_keep(ui);

        /*
         * The one interrupt that remains: the write failed and the original
         * could not be restored, so the next boot may be the one that does not
         * happen. The draft or preview stays behind the panel (R38).
         */
        if (result == EDITOR_WRITE_UNRECOVERED) {
            modal_open_error_titled(
                ui, "DO NOT REBOOT",
                "THE WRITE FAILED AND THE ORIGINAL COULD NOT BE PUT BACK. "
                "THE CONFIG MAY BE INCOMPLETE. DO NOT REBOOT. CHECK "
                "CONFIG.TXT IN VITASHELL.");
        }

        return;
    }

    clear_dock_message(ui);

    /*
     * A restore reparses the snapshot and re-sorts rows section-major. Keeping
     * the numeric index would move the highlight to a different plugin; keep
     * the path instead and fall back only when the row is gone.
     */
    int row = had_selection ? editor_row_for_path(selected_path) : -1;

    ui->selection = row >= 0 ? row : first_row();
    keep_selection_visible(ui);
    plugin_flow_dock_reset(ui);

    if (reboot)
        request_reboot(ui);
}

static void do_preview(UiState *ui, EditorSnapshot kind)
{
    char err[ERR_TEXT_MAX];

    if (editor_preview_restore(kind, err, (int)sizeof(err)) !=
        EDITOR_WRITE_OK) {
        set_dock_message(ui, err);
        plugin_flow_dock_reset(ui);
        return;
    }

    clear_dock_message(ui);
    keep_selection_valid(ui);
    plugin_flow_dock_reset(ui);
}

static void do_discard(UiState *ui)
{
    editor_discard_draft();
    clear_dock_message(ui);
    keep_selection_valid(ui);
    keep_selection_visible(ui);
    plugin_flow_dock_reset(ui);
}

static void do_cancel_preview(UiState *ui)
{
    editor_cancel_preview();
    clear_dock_message(ui);
    keep_selection_valid(ui);
    keep_selection_visible(ui);
    plugin_flow_dock_reset(ui);
}

static void activate_dock(UiState *ui)
{
    PluginFlowCommand command = plugin_flow_focused_command(ui);

    switch (command) {
    case PLUGIN_COMMAND_DISCARD:
        do_discard(ui);
        break;
    case PLUGIN_COMMAND_CANCEL:
        do_cancel_preview(ui);
        break;
    case PLUGIN_COMMAND_SAVE:
    case PLUGIN_COMMAND_CONFIRM:
        do_commit(ui, plugin_nav_commit_mode(command) ==
                              PLUGIN_COMMIT_REBOOT);
        break;
    case PLUGIN_COMMAND_PREVIEW_ORIGINAL:
        do_preview(ui, EDITOR_SNAPSHOT_ORIGINAL);
        break;
    case PLUGIN_COMMAND_PREVIEW_SESSION:
        do_preview(ui, EDITOR_SNAPSHOT_SESSION);
        break;
    case PLUGIN_COMMAND_PREVIEW_UNDO:
        do_preview(ui, EDITOR_SNAPSHOT_UNDO);
        break;
    case PLUGIN_COMMAND_REBOOT:
        request_reboot(ui);
        break;
    case PLUGIN_COMMAND_NONE:
    default:
        break;
    }
}

static InputEvent read_event(const SceCtrlData *pad, InputState *state,
                             const UiState *ui, float dt)
{
    int up = (pad->buttons & SCE_CTRL_UP) != 0;
    int down = (pad->buttons & SCE_CTRL_DOWN) != 0;
    int left = (pad->buttons & SCE_CTRL_LEFT) != 0;
    int right = (pad->buttons & SCE_CTRL_RIGHT) != 0;
    int tab_left = (pad->buttons & SCE_CTRL_LTRIGGER) != 0;
    int tab_right = (pad->buttons & SCE_CTRL_RTRIGGER) != 0;
    int accept = (pad->buttons & SCE_CTRL_CROSS) != 0;
    int cancel = (pad->buttons & SCE_CTRL_CIRCLE) != 0;
    int options = (pad->buttons & SCE_CTRL_TRIANGLE) != 0;
    InputContext context = INPUT_CONTEXT_APP;

    float stick_y = input_stronger_stick(stick_axis(pad->ly),
                                         stick_axis(pad->ry));

    if (ui->modal != UI_MODAL_NONE)
        context = INPUT_CONTEXT_MODAL;

    return input_update(state, up, down, left, right, accept, cancel, options,
                        tab_left, tab_right, stick_y, context, dt);
}

/*
 * Execute the routing decision. app.c keeps the state mutation and the SDK
 * calls; ui/plugin_nav owns which action an event means (KTD4, KTD7).
 */
static void apply_nav_action(UiState *ui, PluginNavAction action)
{
    switch (action) {
    case PLUGIN_NAV_MODAL_DISMISS:
    case PLUGIN_NAV_MODAL_CANCEL:
        close_modal(ui);
        break;

    case PLUGIN_NAV_MODAL_CONFIRM:
        close_modal(ui);
        request_reboot(ui);
        break;

    case PLUGIN_NAV_TAB_LEFT:
        clear_dock_message(ui);
        clear_about_message(ui);
        plugin_flow_focus_list(ui);
        ui->tab = plugin_nav_tab_step(ui->tab, -1);
        if (ui->tab == UI_TAB_PLUGINS) {
            if (ui->selection < 0)
                ui->selection = first_row();
            keep_selection_visible(ui);
        }
        break;

    case PLUGIN_NAV_TAB_RIGHT:
        clear_dock_message(ui);
        clear_about_message(ui);
        plugin_flow_focus_list(ui);
        ui->tab = plugin_nav_tab_step(ui->tab, 1);
        if (ui->tab == UI_TAB_PLUGINS) {
            if (ui->selection < 0)
                ui->selection = first_row();
            keep_selection_visible(ui);
        }
        break;

    case PLUGIN_NAV_VITALS_SCROLL_UP:
        plugin_nav_vitals_step(ui, -1, layout_max_scroll());
        break;
    case PLUGIN_NAV_VITALS_SCROLL_DOWN:
        plugin_nav_vitals_step(ui, 1, layout_max_scroll());
        break;

    case PLUGIN_NAV_SELECT_UP:
        clear_dock_message(ui);
        move_selection(ui, -1);
        break;
    case PLUGIN_NAV_SELECT_DOWN:
        clear_dock_message(ui);
        move_selection(ui, 1);
        break;

    case PLUGIN_NAV_DOCK_ENTER:
        clear_dock_message(ui);
        plugin_flow_dock_enter(ui);
        break;
    case PLUGIN_NAV_DOCK_LEAVE:
        clear_dock_message(ui);
        plugin_flow_focus_list(ui);
        break;
    case PLUGIN_NAV_DOCK_TOGGLE:
        clear_dock_message(ui);
        plugin_flow_dock_toggle(ui);
        break;
    case PLUGIN_NAV_DOCK_LEFT:
        plugin_flow_dock_move(ui, -1);
        break;
    case PLUGIN_NAV_DOCK_RIGHT:
        plugin_flow_dock_move(ui, 1);
        break;

    case PLUGIN_NAV_ACTIVATE_DOCK:
        activate_dock(ui);
        break;
    case PLUGIN_NAV_ACTIVATE_ROW:
        stage_selected_row(ui);
        break;
    case PLUGIN_NAV_ACTIVATE_ABOUT:
        activate_about(ui);
        break;

    case PLUGIN_NAV_NONE:
    default:
        break;
    }
}

static void handle_event(UiState *ui, InputEvent event)
{
    apply_nav_action(ui, plugin_nav_action(ui, event, g_editor.row_count,
                                           editor_preview_active()));
}

int app_run(void)
{
    if (sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG) < 0)
        s_analog_ready = 0;

    /*
     * Snapshot the config set before anything reads it. The editor's scan also
     * resolves the active file, so the launch set and the editable set agree.
     */
    editor_scan();
    plugins_scan();
    metrics_init();

    UiState ui;
    memset(&ui, 0, sizeof(ui));
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = first_row();
    ui.modal = UI_MODAL_NONE;

    /*
     * The safety card is the first screen. It is composed after the scan and
     * snapshot attempt, so every sentence describes what this launch actually
     * did, and it owns input until the user acknowledges it.
     */
    LaunchCard card;
    launch_card_build(&card);
    modal_open_safety(&ui, &card);

    InputState input;
    input_reset(&input);

    uint64_t last = sceKernelGetProcessTimeWide();
    double slow_acc = 0.0;
    int first_frame = 1;

    for (;;) {
        uint64_t now = sceKernelGetProcessTimeWide();
        float raw_dt = (float)((double)(now - last) / MICROS_PER_SECOND);
        last = now;

        float dt = raw_dt;
        if (dt > MAX_FRAME_DT)
            dt = MAX_FRAME_DT;

        SceCtrlData pad;
        memset(&pad, 0, sizeof(pad));

        /*
         * A failed read leaves the pad zeroed, which would look like both
         * sticks at full up-travel. Center the axes so the screen holds still
         * instead of scrolling (rule 6).
         */
        int pad_read = sceCtrlPeekBufferPositive(0, &pad, 1);

        if (pad_read <= 0 || !s_analog_ready) {
            memset(&pad, 0, sizeof(pad));
            pad.lx = 128;
            pad.ly = 128;
            pad.rx = 128;
            pad.ry = 128;
        }

        InputEvent event = read_event(&pad, &input, &ui, dt);
        if (!first_frame)
            handle_event(&ui, event);
        first_frame = 0;

        if (ui.focus != UI_FOCUS_DOCK)
            plugin_nav_apply_scroll(&ui, input.scroll_y, dt,
                                    layout_max_scroll(), plugins_max_scroll());

        metrics_sample_frame(dt);

        slow_acc += (double)dt;

        int caught_up = 0;
        while (slow_acc >= SLOW_SAMPLE_PERIOD &&
               caught_up < MAX_CATCH_UP_SAMPLES) {
            metrics_sample_slow();
            slow_acc -= SLOW_SAMPLE_PERIOD;
            ++caught_up;
        }

        if (slow_acc >= SLOW_SAMPLE_PERIOD)
            slow_acc = 0.0;

        plugin_flow_revalidate_focus(&ui);

        /*
         * The gate is read every frame. A battery that crosses the floor while
         * the confirmation is open closes it with the reason instead of
         * letting a stale open confirmation send a request.
         */
        if (ui.modal == UI_MODAL_REBOOT) {
            const char *block = plugin_flow_reboot_block_reason();

            if (block != NULL) {
                close_modal(&ui);
                set_about_message(&ui, block);
            }
        }

        vita2d_start_drawing();
        vita2d_clear_screen();
        screen_draw((float)((double)now / MICROS_PER_SECOND), &ui);
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }
}
