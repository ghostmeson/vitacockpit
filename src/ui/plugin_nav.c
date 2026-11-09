#include "ui/plugin_nav.h"

#include <stddef.h>

static float clamp_scroll(float value, int max)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > (float)max)
        return (float)max;

    return value;
}

PluginNavAction plugin_nav_action(const UiState *ui, InputEvent event,
                                  int row_count, int preview_active)
{
    if (ui == NULL)
        return PLUGIN_NAV_NONE;

    if (ui->modal != UI_MODAL_NONE) {
        /*
         * The reboot panel is the only modal with two outcomes. Every other
         * modal is an acknowledgement: Cross and Circle both close it.
         */
        if (ui->modal == UI_MODAL_REBOOT) {
            if (event == INPUT_ACCEPT)
                return PLUGIN_NAV_MODAL_CONFIRM;
            if (event == INPUT_CANCEL)
                return PLUGIN_NAV_MODAL_CANCEL;
            return PLUGIN_NAV_NONE;
        }

        return (event == INPUT_ACCEPT || event == INPUT_CANCEL)
                   ? PLUGIN_NAV_MODAL_DISMISS
                   : PLUGIN_NAV_NONE;
    }

    switch (event) {
    case INPUT_TAB_LEFT:
        return PLUGIN_NAV_TAB_LEFT;
    case INPUT_TAB_RIGHT:
        return PLUGIN_NAV_TAB_RIGHT;

    case INPUT_OPTIONS:
        /* Triangle is dock-only. */
        return ui->tab == UI_TAB_PLUGINS ? PLUGIN_NAV_DOCK_TOGGLE
                                         : PLUGIN_NAV_NONE;

    case INPUT_LEFT:
    case INPUT_RIGHT:
        if (ui->tab == UI_TAB_PLUGINS && ui->focus == UI_FOCUS_DOCK)
            return event == INPUT_LEFT ? PLUGIN_NAV_DOCK_LEFT
                                       : PLUGIN_NAV_DOCK_RIGHT;
        return PLUGIN_NAV_NONE;

    case INPUT_CANCEL:
        if (ui->tab == UI_TAB_PLUGINS && ui->focus == UI_FOCUS_DOCK)
            return PLUGIN_NAV_DOCK_LEAVE;
        return PLUGIN_NAV_NONE;

    case INPUT_UP:
    case INPUT_DOWN:
        if (ui->tab == UI_TAB_ABOUT)
            return PLUGIN_NAV_NONE;
        if (ui->tab == UI_TAB_VITALS)
            return event == INPUT_UP ? PLUGIN_NAV_VITALS_SCROLL_UP
                                     : PLUGIN_NAV_VITALS_SCROLL_DOWN;

        if (ui->focus == UI_FOCUS_DOCK)
            return event == INPUT_UP ? PLUGIN_NAV_DOCK_LEAVE
                                     : PLUGIN_NAV_NONE;

        if (preview_active)
            return event == INPUT_DOWN ? PLUGIN_NAV_DOCK_ENTER
                                       : PLUGIN_NAV_NONE;

        /*
         * With no rows the dock is the only surface below, so Down enters it.
         * Up has nowhere to go.
         */
        if (row_count <= 0)
            return event == INPUT_DOWN ? PLUGIN_NAV_DOCK_ENTER
                                       : PLUGIN_NAV_NONE;

        if (event == INPUT_DOWN && ui->selection >= row_count - 1)
            return PLUGIN_NAV_DOCK_ENTER;
        if (event == INPUT_UP && ui->selection <= 0)
            return PLUGIN_NAV_NONE;

        return event == INPUT_UP ? PLUGIN_NAV_SELECT_UP
                                 : PLUGIN_NAV_SELECT_DOWN;

    case INPUT_ACCEPT:
        if (ui->tab != UI_TAB_PLUGINS)
            return PLUGIN_NAV_NONE;
        if (ui->focus == UI_FOCUS_DOCK)
            return PLUGIN_NAV_ACTIVATE_DOCK;
        if (preview_active || ui->selection < 0)
            return PLUGIN_NAV_NONE;
        return PLUGIN_NAV_ACTIVATE_ROW;

    default:
        return PLUGIN_NAV_NONE;
    }
}

PluginCommitMode plugin_nav_commit_mode(PluginFlowCommand command)
{
    switch (command) {
    case PLUGIN_COMMAND_SAVE:
        return PLUGIN_COMMIT_SAVE;
    case PLUGIN_COMMAND_CONFIRM:
        return PLUGIN_COMMIT_REBOOT;
    case PLUGIN_COMMAND_NONE:
    case PLUGIN_COMMAND_DISCARD:
    case PLUGIN_COMMAND_CANCEL:
    case PLUGIN_COMMAND_PREVIEW_ORIGINAL:
    case PLUGIN_COMMAND_PREVIEW_SESSION:
    case PLUGIN_COMMAND_PREVIEW_UNDO:
    case PLUGIN_COMMAND_REBOOT:
    default:
        return PLUGIN_COMMIT_NONE;
    }
}

UiTab plugin_nav_tab_step(UiTab tab, int direction)
{
    int count = (int)UI_TAB_COUNT;
    int index = (int)tab;

    if (index < 0 || index >= count)
        index = 0;

    index += direction < 0 ? count - 1 : 1;
    return (UiTab)(index % count);
}

void plugin_nav_scroll_vitals(UiState *ui, float delta, int max)
{
    if (ui != NULL)
        ui->vitals_scroll = clamp_scroll(ui->vitals_scroll + delta, max);
}

void plugin_nav_scroll_plugins(UiState *ui, float delta, int max)
{
    if (ui != NULL)
        ui->plugins_scroll = clamp_scroll(ui->plugins_scroll + delta, max);
}

void plugin_nav_set_plugins_scroll(UiState *ui, float target, int max)
{
    if (ui != NULL)
        ui->plugins_scroll = clamp_scroll(target, max);
}

void plugin_nav_apply_scroll(UiState *ui, float scroll_y, float dt,
                             int vitals_max, int plugins_max)
{
    if (ui == NULL)
        return;

    float delta = scroll_y * dt;

    if (ui->tab == UI_TAB_VITALS)
        plugin_nav_scroll_vitals(ui, delta, vitals_max);
    else if (ui->tab == UI_TAB_PLUGINS)
        plugin_nav_scroll_plugins(ui, delta, plugins_max);
}

void plugin_nav_vitals_step(UiState *ui, int direction, int max)
{
    if (ui == NULL)
        return;

    plugin_nav_scroll_vitals(ui, direction < 0 ? -PLUGIN_NAV_VITALS_STEP
                                               : PLUGIN_NAV_VITALS_STEP,
                             max);
}
