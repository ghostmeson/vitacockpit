#include "ui/about_text.h"

#include <stdio.h>

#include "core/plugin_editor.h"

void about_reboot_text(char *title, size_t title_cap, char *body,
                       size_t body_cap)
{
    const char *heading = "REBOOT";
    const char *text = "REBOOT THE CONSOLE?";

    if (editor_preview_active()) {
        heading = "REBOOT AND DISCARD";

        if (editor_preview_discards_draft())
            text = "THE RESTORE PREVIEW AND STAGED DRAFT WILL BE DISCARDED. "
                   "APPLY THE RESTORE WITH DISCARD DRAFT & RESTORE IN "
                   "PLUGINS. REBOOT WITHOUT APPLYING?";
        else
            text = "THE RESTORE PREVIEW WILL BE DISCARDED. APPLY IT WITH "
                   "CONFIRM & REBOOT IN PLUGINS. REBOOT WITHOUT APPLYING?";
    } else if (editor_draft_dirty()) {
        heading = "REBOOT AND DISCARD";
        text = "STAGED PLUGIN CHANGES WILL BE DISCARDED. APPLY THEM WITH "
               "CONFIRM & REBOOT IN PLUGINS. REBOOT WITHOUT APPLYING?";
    } else if (g_editor.pending_count > 0) {
        text = "THE SAVED CONFIG WILL BE APPLIED AT NEXT BOOT. REBOOT NOW?";
    }

    if (title != NULL && title_cap > 0)
        snprintf(title, title_cap, "%s", heading);
    if (body != NULL && body_cap > 0)
        snprintf(body, body_cap, "%s", text);
}
