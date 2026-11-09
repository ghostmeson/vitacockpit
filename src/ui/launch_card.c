#include "ui/launch_card.h"

#include <stdio.h>
#include <string.h>

#include "core/plugin_editor.h"

/*
 * The first screen of the app. Every sentence here is a claim about what this
 * launch actually did, so the body is composed from the editor's launch result
 * rather than from `backup_warning` or the readable-snapshot probes. A failed
 * snapshot must be named, never smoothed over: this card is what tells a new
 * user whether they have a restore point.
 */

#define CARD_TITLE "CONFIG SAFETY"

static void append(char *dst, size_t cap, const char *text)
{
    size_t used = strlen(dst);

    if (used >= cap)
        return;

    snprintf(dst + used, cap - used, "%s", text);
}

static void append_locked_reason(char *body, size_t cap)
{
    const char *reason = editor_readonly_reason();

    append(body, cap, "EDITING IS LOCKED");

    if (reason != NULL) {
        append(body, cap, ": ");
        append(body, cap, reason);
    }

    append(body, cap, ". ");
}

static void build_no_config(char *body, size_t cap)
{
    append(body, cap,
           "COCKPIT MANAGES THE ACTIVE TAIHEN CONFIG.TXT. NO CONFIG WAS "
           "FOUND, SO ");
    append_locked_reason(body, cap);
    append(body, cap, "NO SESSION SNAPSHOT WAS WRITTEN. ");

    if (editor_can_restore())
        append(body, cap, "A RESTORE SNAPSHOT IS AVAILABLE IN PLUGINS.");
    else
        append(body, cap, "NO RESTORE SNAPSHOT IS AVAILABLE.");
}

void launch_card_build(LaunchCard *out)
{
    if (out == NULL)
        return;

    snprintf(out->title, sizeof(out->title), "%s", CARD_TITLE);
    out->body[0] = '\0';

    /*
     * The read-failure cases and the no-config case have different restore
     * answers, so they get their own bodies. A single sentence cannot be true
     * in all of them, and a false restore promise is worse than no promise.
     */
    if (!g_editor.source_ok) {
        build_no_config(out->body, sizeof(out->body));
        return;
    }

    append(out->body, sizeof(out->body),
           "COCKPIT MANAGES THE ACTIVE TAIHEN CONFIG.TXT. MANUAL EDITS ARE "
           "RISKY AND HARD TO UNDO. ENABLING AND DISABLING HERE IS SAFER THAN "
           "A MANUAL EDIT. ");

    if (editor_session_snapshot_written()) {
        append(out->body, sizeof(out->body),
               "A SESSION SNAPSHOT WAS WRITTEN. ");

        if (editor_original_snapshot_present())
            append(out->body, sizeof(out->body), "THE ORIGINAL IS PRESENT.");
        else
            append(out->body, sizeof(out->body),
                   "NO ORIGINAL SNAPSHOT IS PRESENT.");
    } else {
        append(out->body, sizeof(out->body),
               "NO SESSION SNAPSHOT WAS WRITTEN. ");

        if (editor_original_snapshot_present())
            append(out->body, sizeof(out->body),
                   "THE ORIGINAL IS PRESENT, BUT THIS SESSION HAS NO NEW "
                   "RESTORE POINT.");
        else
            append(out->body, sizeof(out->body),
                   "NO ORIGINAL SNAPSHOT IS PRESENT. THIS SESSION HAS NO "
                   "RESTORE POINT.");
    }

    if (editor_readonly_reason() != NULL) {
        append(out->body, sizeof(out->body), " ");
        append_locked_reason(out->body, sizeof(out->body));
    }
}
