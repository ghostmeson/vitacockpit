#ifndef COCKPIT_UI_UI_STATE_H
#define COCKPIT_UI_UI_STATE_H

/*
 * The screen's navigation state. The app owns it; screen_draw and the panels
 * read it. Keeping it out of the panels is what lets HOME and PLUGINS keep
 * independent scroll positions (KTD1).
 */

typedef enum {
    UI_TAB_HOME = 0,
    UI_TAB_PLUGINS,
    UI_TAB_COUNT
} UiTab;

typedef enum {
    UI_FOCUS_LIST = 0,
    UI_FOCUS_DOCK
} UiFocus;

typedef enum {
    UI_MODAL_NONE = 0,
    /*
     * The only interrupt left. Everything a dock action can recover from is
     * reported inline; this panel is reserved for a write whose original could
     * not be put back (R38).
     */
    UI_MODAL_ERROR,
} UiModal;

typedef struct {
    UiTab tab;
    float home_scroll;
    float plugins_scroll;
    int selection; /* row index in g_editor */

    /*
     * PLUGINS transaction dock. `ui/plugin_flow` derives the state and the
     * action list; these fields carry only the cursor and the inline error.
     * `dock_message` is the app's transient failure copy; the flow fills the
     * same line from state when it is empty.
     */
    UiFocus focus;
    int dock_choice;
    char dock_message[128];

    UiModal modal;
    char modal_title[64];
    char modal_body[512];
} UiState;

#endif
