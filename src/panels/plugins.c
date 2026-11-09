#include "panels/plugins.h"

#include <stdio.h>
#include <string.h>

#include "core/plugin_editor.h"
#include "core/tai_config.h"
#include "gfx/font.h"
#include "gfx/layout.h"
#include "gfx/theme.h"
#include "ui/plugin_flow.h"

/*
 * The editable ledger (D57, D62, D66) and the inline transaction dock.
 *
 * The panel draws three truths side by side: the working set (title and split
 * rocker), the launch set (the status badge), and the pending boot set (NEXT
 * ON / NEXT OFF). `ui/plugin_flow` owns which dock state those facts produce;
 * this file only lays it out. See docs/DESIGN.md and R8-R32.
 */

#define PLUGIN_TOP (PANEL_TITLE_Y + LINE_H(PANEL_TITLE_PX) + SP_4)
#define PLUGIN_STRIDE PLUGINS_ROW_STRIDE
#define PLUGIN_DESC_DY 22.0f
#define PLUGIN_LINE_H 14.0f
#define GROUP_HEADER_H 26.0f
#define GROUP_GAP 10.0f
#define PLUGIN_BOTTOM 16.0f
#define HEADER_LINE_H 14.0f
#define HEADER_LINE_COUNT 3

/* The ledger starts under the three-line header. */
#define ROWS_TOP (PLUGIN_TOP + HEADER_LINE_H * HEADER_LINE_COUNT + SP_2)
#define PLUGIN_EMPTY_H 30.0f

/* PLUGINS owns the bottom of the screen: the frame strip is HOME-only. */
#define PLUGINS_VIEW_H (SCREEN_H - VIEW_Y - 8)

/* The row control: a split rocker, not a labelled action box (R17). */
#define ROCKER_W 46.0f
#define ROCKER_H 20.0f
#define ROCKER_GAP 2.0f
#define ROCKER_BADGE_GAP 4.0f

#define BADGE_H 12.0f
#define BADGE_PAD 5.0f
#define BADGE_GAP 6.0f
#define DRAFT_NOTCH 6.0f

/*
 * A 4 % white wash. draw_dim() scales RGB and keeps alpha, which would make a
 * "4 % white" darker than the panel; the wash has to be alpha instead.
 */
#define SELECT_WASH COCKPIT_RGBA8(0xED, 0xED, 0xF0, 0x0A)

/*
 * Pinned dock layout. Heights live in ui/plugin_flow (64 / 159 / 123 / 111).
 * The state block has 12 px of air above and below it; the action area is a
 * row of pressable slabs: a 34 px primary row, a 28 px secondary row, and a
 * warning line with its own yellow marker.
 */
#define DOCK_TITLE_DY 12.0f
#define DOCK_SUMMARY_DY 32.0f
#define DOCK_RULE_DY 48.0f
#define DOCK_ACTION_DY 54.0f
#define DOCK_SECONDARY_DY 100.0f
#define DOCK_WARNING_DY 136.0f
#define DOCK_PREVIEW_ACTION_DY 58.0f
#define DOCK_PREVIEW_WARNING_DY 100.0f
#define DOCK_SAVED_TITLE_DY 12.0f
#define DOCK_SAVED_RULE_DY 34.0f
#define DOCK_SAVED_ACTION_DY 46.0f
#define DOCK_SAVED_WARNING_DY 88.0f
#define DOCK_IDLE_ACTION_DY 15.0f
#define DOCK_BTN_H 34.0f
#define DOCK_BTN_SMALL_H 28.0f
#define DOCK_BTN_PAD 12.0f
#define DOCK_BTN_GAP 10.0f
#define DOCK_BTN_LEDGE 3.0f
#define DOCK_SEGMENT_GAP 16.0f

#define SOURCE_RESERVE 420.0f
#define DIFF_LABEL_RESERVE 110.0f

#define KEYCAP_W 14.0f
#define EXCEPTION_SEGMENTS 5

#define TEXT_FIT_MAX 192

static int rows_in_section(PluginSection section)
{
    int n = 0;

    for (int i = 0; i < g_editor.row_count; ++i) {
        if (g_editor.rows[i].section == section)
            ++n;
    }

    return n;
}

static int group_count(void)
{
    int n = 0;

    for (int s = 0; s < PLUGIN_SECTION_COUNT; ++s) {
        if (rows_in_section((PluginSection)s) > 0)
            ++n;
    }

    return n;
}

float plugins_content_height(void)
{
    float h = ROWS_TOP;

    if (editor_preview_active()) {
        int groups = group_count();
        int removed = editor_preview_removed_count();

        h += (float)groups * GROUP_HEADER_H +
             (float)g_editor.row_count * PLUGIN_STRIDE;

        if (groups > 1)
            h += (float)(groups - 1) * GROUP_GAP;

        if (removed > 0)
            h += GROUP_GAP + GROUP_HEADER_H +
                 (float)removed * PLUGIN_LINE_H;

        return h + PLUGIN_BOTTOM;
    }

    if (g_editor.row_count == 0) {
        h += PLUGIN_EMPTY_H;
    } else {
        int groups = group_count();

        h += (float)groups * GROUP_HEADER_H +
             (float)g_editor.row_count * PLUGIN_STRIDE;

        if (groups > 1)
            h += (float)(groups - 1) * GROUP_GAP;
    }

    if (g_editor.orphan_count > 0 || g_editor.orphans_unverified) {
        h += GROUP_GAP + GROUP_HEADER_H + PLUGIN_LINE_H;
        h += (float)g_editor.orphan_count * PLUGIN_LINE_H;
        if (g_editor.orphans_unverified)
            h += PLUGIN_LINE_H;
        if (g_editor.orphan_truncated)
            h += PLUGIN_LINE_H;
    }

    if (g_editor.truncated)
        h += GROUP_GAP + PLUGIN_LINE_H;

    return h + PLUGIN_BOTTOM;
}

int plugins_row_y(int row)
{
    float y = ROWS_TOP;
    int groups_drawn = 0;

    if (g_editor.row_count == 0)
        return (int)y;

    for (int s = 0; s < PLUGIN_SECTION_COUNT; ++s) {
        PluginSection section = (PluginSection)s;

        if (rows_in_section(section) == 0)
            continue;

        if (groups_drawn > 0)
            y += GROUP_GAP;

        y += GROUP_HEADER_H;
        ++groups_drawn;

        for (int i = 0; i < g_editor.row_count; ++i) {
            if (g_editor.rows[i].section != section)
                continue;

            if (i == row)
                return (int)y;

            y += PLUGIN_STRIDE;
        }
    }

    return (int)y;
}

float plugins_dock_height(void)
{
    return (float)plugin_flow_dock_height(plugin_flow_state());
}

int plugins_viewport_height(void)
{
    return PLUGINS_VIEW_H - VIEW_TOP_GAP - (int)plugins_dock_height();
}

float plugins_list_bottom(void)
{
    return (float)(VIEW_Y + PLUGINS_VIEW_H) - plugins_dock_height();
}

int plugins_max_scroll(void)
{
    int available = plugins_viewport_height();
    int max = (int)plugins_content_height() - available;

    return max > 0 ? max : 0;
}

static uint32_t status_colour(EditorStatus status)
{
    switch (status) {
    case EDITOR_LOADED:
        return C_GREEN;
    case EDITOR_MISSING:
    case EDITOR_PARTIAL:
        return C_YELLOW;
    case EDITOR_BOOT:
        return C_CYAN;
    case EDITOR_DISABLED:
        return C_RED;
    case EDITOR_NOT_REACHED:
    default:
        return C_DIM;
    }
}

static float badge_width(const char *text)
{
    return font_width(text, PX_MICRO) + BADGE_PAD * 2.0f;
}

static void draw_badge(float x, float y, const char *text, uint32_t colour)
{
    float w = badge_width(text);

    draw_rect(x, y, w, BADGE_H, draw_dim(colour, 0.16f));
    font_text(text, x + BADGE_PAD, y + 2.0f, PX_MICRO, colour);
}

/*
 * Truncate `text` with two ASCII periods. The font has no ellipsis glyph and
 * the UI labels are ASCII only (KTD6), so `..` is the one accepted form.
 */
static void draw_text_fit(const char *text, float x, float right, float y,
                          float px, uint32_t colour)
{
    if (text == NULL || right <= x)
        return;

    float avail = right - x;

    if (font_width(text, px) <= avail) {
        font_text(text, x, y, px, colour);
        return;
    }

    char buf[TEXT_FIT_MAX];
    size_t n = strlen(text);

    if (n > sizeof(buf) - 4)
        n = sizeof(buf) - 4;

    memcpy(buf, text, n);
    buf[n] = '\0';

    float dots = font_width("..", px);

    while (n > 0 && font_width(buf, px) + dots > avail) {
        --n;
        buf[n] = '\0';
    }

    memcpy(buf + n, "..", 3);
    font_text(buf, x, y, px, colour);
}

static float draw_token(float x, float y, const char *text, uint32_t colour)
{
    font_text(text, x, y, PX_MICRO, colour);
    return x + font_width(text, PX_MICRO);
}

static float draw_pipe(float x, float y)
{
    return draw_token(x + SP_1, y, "|", C_DIM) + SP_1;
}

static void draw_source_line(Rect r, float y)
{
    float left = r.x + PANEL_PAD;
    float right = r.x + r.w - PANEL_PAD;
    float x = left;

    x = draw_token(x, y, "USING", C_DIM) + SP_2;

    const char *path = g_editor.source[0] != '\0' ? g_editor.source : "--";
    float path_right = right - SOURCE_RESERVE;
    float path_x = x;

    draw_text_fit(path, path_x, path_right, y, PX_MICRO,
                  g_editor.source[0] != '\0' ? C_CYAN : C_DIM);
    x += font_width(path, PX_MICRO);
    if (x > path_right)
        x = path_right;

    if (editor_preview_active()) {
        x = draw_pipe(x, y);
        x = draw_token(x, y, "PREVIEW", C_YELLOW);
    } else if (editor_draft_dirty()) {
        x = draw_pipe(x, y);
        x = draw_token(x, y, "DRAFT", C_YELLOW);
    }

    x = draw_pipe(x, y);

    char number[16];

    snprintf(number, sizeof(number), "%d", g_editor.active_total);
    x = draw_token(x, y, number,
                   g_editor.active_total > 0 ? C_GREEN : C_DIM);
    x = draw_token(x + SP_1, y, "ACTIVE", C_DIM);

    x = draw_pipe(x, y);

    snprintf(number, sizeof(number), "%d", g_editor.disabled_total);
    x = draw_token(x, y, number,
                   g_editor.disabled_total > 0 ? C_RED : C_DIM);
    x = draw_token(x + SP_1, y, "DISABLED", C_DIM);

    if (g_editor.pending_count > 0) {
        x = draw_pipe(x, y);
        snprintf(number, sizeof(number), "%d", g_editor.pending_count);
        x = draw_token(x, y, number, C_YELLOW);
        draw_token(x + SP_1, y, "NEXT BOOT", C_YELLOW);
    }
}

/*
 * The exception slot (R12, KD11). Segments are ordered by precedence and the
 * line appends only while it fits; the last one is cut with `..`. The slot is
 * always present, so a warning never shifts the ledger below it.
 */
static void draw_exception_line(Rect r, float y)
{
    float left = r.x + PANEL_PAD;
    float right = r.x + r.w - PANEL_PAD;
    float x = left;

    const char *segments[EXCEPTION_SEGMENTS];
    int count = 0;
    char shadow[TEXT_FIT_MAX];

    const char *reason = editor_readonly_reason();
    if (reason != NULL && count < EXCEPTION_SEGMENTS)
        segments[count++] = reason;

    if ((g_editor.invalid || g_editor.paths_truncated) &&
        count < EXCEPTION_SEGMENTS)
        segments[count++] = "CONFIG WARNING";

    if (g_editor.backup_warning && count < EXCEPTION_SEGMENTS)
        segments[count++] = "BACKUP WARNING";

    if (g_editor.ignored[0] != '\0' && count < EXCEPTION_SEGMENTS) {
        snprintf(shadow, sizeof(shadow), "SHADOWED %s", g_editor.ignored);
        segments[count++] = shadow;
    }

    if (g_editor.recovered && count < EXCEPTION_SEGMENTS)
        segments[count++] = "RECOVERED";

    for (int i = 0; i < count; ++i) {
        float sep = x > left
                        ? SP_1 + font_width("|", PX_MICRO) + SP_1
                        : 0.0f;

        if (x + sep + font_width(segments[i], PX_MICRO) <= right) {
            if (sep > 0.0f)
                x = draw_pipe(x, y);

            font_text(segments[i], x, y, PX_MICRO, C_YELLOW);
            x += font_width(segments[i], PX_MICRO);
            continue;
        }

        if (sep > 0.0f && x + sep < right)
            x = draw_pipe(x, y);

        draw_text_fit(segments[i], x, right, y, PX_MICRO, C_YELLOW);
        break;
    }
}

static void draw_keycap(float x, float y)
{
    const float w = KEYCAP_W;
    const float h = 12.0f;
    const float cut = 3.0f;

    draw_rect(x, y, w, h, C_HAIR);
    draw_tri(x + w - cut, y, x + w, y, x + w, y + cut, C_PANEL);

    draw_line(x, y, x + w - cut, y, C_DIM);
    draw_line(x + w - cut, y, x + w, y + cut, C_DIM);
    draw_line(x + w, y + cut, x + w, y + h, C_DIM);
    draw_line(x, y + h, x + w, y + h, C_DIM);
    draw_line(x, y, x, y + h, C_DIM);

    /* White outline triangle, apex up: the PlayStation triangle shortcut
     * (R13). Three 1 px lines inside the keycap. */
    float apex_x = x + 7.0f;
    float apex_y = y + 3.0f;
    float base_y = y + 9.0f;

    draw_line(apex_x, apex_y, x + 4.0f, base_y, C_WHITE);
    draw_line(apex_x, apex_y, x + 10.0f, base_y, C_WHITE);
    draw_line(x + 4.0f, base_y, x + 10.0f, base_y, C_WHITE);
}

static void draw_prompt_line(Rect r, float y)
{
    float x = r.x + PANEL_PAD;

    x = draw_token(x, y, "PRESS", C_DIM) + SP_2;
    draw_keycap(x, y + 1.0f);
    x += KEYCAP_W + SP_2;
    draw_token(x, y, "FOR DOCK", C_DIM);
}

static void draw_header_lines(Rect r)
{
    float y = r.y + PLUGIN_TOP;

    draw_source_line(r, y);
    y += HEADER_LINE_H;
    draw_exception_line(r, y);
    y += HEADER_LINE_H;
    draw_prompt_line(r, y);
}

static void draw_name(const char *label, float x, float y, float avail,
                      uint32_t colour)
{
    char display[PLUGIN_PATH_MAX];
    float px = PX_LABEL;

    tai_copy(display, sizeof(display), label);

    if (font_width(display, px) > avail) {
        px = PX_MICRO;
        if (font_width(display, px) > avail) {
            int max_chars = (int)((avail + px) / (6.0f * px));
            if (max_chars < 3)
                max_chars = 3;
            if (max_chars < (int)sizeof(display)) {
                display[max_chars - 2] = '.';
                display[max_chars - 1] = '.';
                display[max_chars] = '\0';
            }
        }
    }

    font_text(display, x, y, px, colour);
}

/*
 * The split rocker. The left half is the off state, the right half the on
 * state, and a mixed row lights both. A row that cannot be switched keeps the
 * same shape at 45 % so the control still explains itself (R17, R52).
 */
static void draw_rocker(float x, float y, const EditorRow *p, int actionable,
                        int selected)
{
    float half = (ROCKER_W - ROCKER_GAP) * 0.5f;
    uint32_t track = draw_dim(C_HAIR, 0.75f);
    uint32_t on_colour = actionable ? C_GREEN : draw_dim(C_GREEN, 0.45f);
    uint32_t off_colour = actionable ? C_RED : draw_dim(C_RED, 0.45f);
    int on = p->active_lines > 0;
    int off = p->active_lines < p->reached_lines;

    draw_rect(x, y, half, ROCKER_H, off ? off_colour : track);
    draw_rect(x + half + ROCKER_GAP, y, half, ROCKER_H,
              on ? on_colour : track);

    uint32_t outline = selected ? C_WHITE : C_HAIR;

    draw_line(x, y, x + ROCKER_W, y, outline);
    draw_line(x + ROCKER_W, y, x + ROCKER_W, y + ROCKER_H, outline);
    draw_line(x, y + ROCKER_H, x + ROCKER_W, y + ROCKER_H, outline);
    draw_line(x, y, x, y + ROCKER_H, outline);
}

static void draw_draft_notch(float x, float y)
{
    draw_tri(x + ROCKER_W - DRAFT_NOTCH, y, x + ROCKER_W, y,
             x + ROCKER_W, y + DRAFT_NOTCH, C_YELLOW);
}

static void draw_row(const UiState *ui, Rect r, int index, float y)
{
    const EditorRow *p = &g_editor.rows[index];
    float left = r.x + PANEL_PAD;
    float right = r.x + r.w - PANEL_PAD;
    const char *label = editor_row_label(index);
    int action = editor_next_action(index);
    uint32_t title_colour = p->active_lines > 0 ? C_GREEN_DIM : C_WHITE;

    if (index == ui->selection) {
        draw_rect(left, y - 4.0f, right - left, PLUGIN_STRIDE - 6.0f,
                  SELECT_WASH);
        draw_rect(r.x + 4.0f, y - 2.0f, 2.0f, PLUGIN_STRIDE - 6.0f, C_WHITE);
    }

    draw_name(label, left, y, right - ROCKER_W - SP_3 - left, title_colour);
    draw_rocker(right - ROCKER_W, y - 4.0f, p, action != EDITOR_ACTION_NONE,
                index == ui->selection);

    if (p->draft_change != 0)
        draw_draft_notch(right - ROCKER_W, y - 4.0f);

    const char *status = editor_status_text(p->launch_status);
    float badge_y = y + PLUGIN_DESC_DY - 2.0f;
    float badge_x = right - badge_width(status);

    draw_badge(badge_x, badge_y, status, status_colour(p->launch_status));

    if (p->locked) {
        badge_x -= BADGE_GAP + badge_width("LOCKED");
        draw_badge(badge_x, badge_y, "LOCKED", C_DIM);
    }

    if (p->next_change != 0) {
        const char *next = p->next_change > 0 ? "NEXT ON" : "NEXT OFF";

        badge_x -= BADGE_GAP + badge_width(next);
        draw_badge(badge_x, badge_y, next, C_YELLOW);
    }

    float desc_right = badge_x - ROCKER_BADGE_GAP;

    draw_text_fit(p->desc[0] != '\0' ? p->desc : "N/A", left, desc_right,
                  y + PLUGIN_DESC_DY, PX_MICRO, C_DIM);
}

static void draw_preview_row(Rect r, int index, float y)
{
    const EditorRow *p = &g_editor.rows[index];
    float left = r.x + PANEL_PAD;
    float right = r.x + r.w - PANEL_PAD;
    EditorDiff diff = editor_preview_diff(index);
    const char *marker = NULL;
    uint32_t marker_colour = C_DIM;
    int changed = diff != EDITOR_DIFF_UNCHANGED;
    float name_x = left;

    if (diff == EDITOR_DIFF_ENABLE) {
        marker = "+";
        marker_colour = C_GREEN;
    } else if (diff == EDITOR_DIFF_DISABLE) {
        marker = "-";
        marker_colour = C_YELLOW;
    } else if (diff == EDITOR_DIFF_MOVE) {
        marker = ">";
        marker_colour = C_YELLOW;
    }

    if (marker != NULL) {
        font_text(marker, left, y, PX_LABEL, marker_colour);
        name_x += FONT_ADVANCE * PX_LABEL + SP_1;
    }

    uint32_t title_colour = changed ? C_WHITE : draw_dim(C_WHITE, 0.55f);

    draw_name(editor_row_label(index), name_x, y,
              right - DIFF_LABEL_RESERVE - name_x, title_colour);

    const char *label = editor_diff_text(diff);

    if (label != NULL) {
        font_text_right(label, right, y + 3.0f, PX_MICRO,
                        diff == EDITOR_DIFF_ENABLE ? C_GREEN : C_YELLOW);
    }

    uint32_t desc_colour = changed ? C_DIM : draw_dim(C_DIM, 0.70f);

    draw_text_fit(p->desc[0] != '\0' ? p->desc : "N/A", name_x,
                  right, y + PLUGIN_DESC_DY, PX_MICRO, desc_colour);
}

static void draw_empty_state(Rect r)
{
    float y = r.y + ROWS_TOP;

    if (g_editor.source[0] == '\0') {
        font_text("NO CONFIG.TXT FOUND", r.x + PANEL_PAD, y, PX_MICRO,
                  C_YELLOW);
        font_text("EXPECTED UR0:TAI OR UX0:TAI", r.x + PANEL_PAD,
                  y + PLUGIN_LINE_H, PX_MICRO, C_DIM);
    } else {
        font_text("NO OTHER PLUGIN ENTRIES IN THE ACTIVE CONFIG",
                  r.x + PANEL_PAD, y, PX_MICRO, C_DIM);
    }
}

static int visible_in_list(float y, float h, float list_bottom)
{
    return y + h > (float)VIEW_Y && y < list_bottom;
}

static void draw_edit_list(const UiState *ui, Rect r, float list_bottom)
{
    int visible = g_editor.row_count;

    if (visible == 0)
        draw_empty_state(r);

    float y = r.y + ROWS_TOP + (visible == 0 ? PLUGIN_EMPTY_H : 0.0f);
    PluginSection group = PLUGIN_SECTION_COUNT;

    for (int i = 0; i < g_editor.row_count; ++i) {
        const EditorRow *row = &g_editor.rows[i];

        if (row->section != group) {
            if (group != PLUGIN_SECTION_COUNT)
                y += GROUP_GAP;

            group = row->section;

            if (visible_in_list(y, LINE_H(PX_MICRO), list_bottom))
                font_text_tracked(editor_section_text(group), r.x + PANEL_PAD,
                                  y, PX_MICRO, C_CYAN, 1.0f);

            y += GROUP_HEADER_H;
        }

        if (visible_in_list(y, PLUGIN_DESC_DY + PLUGIN_LINE_H, list_bottom))
            draw_row(ui, r, i, y);

        y += PLUGIN_STRIDE;
    }

    if (g_editor.orphan_count > 0 || g_editor.orphans_unverified) {
        y += GROUP_GAP;
        if (visible_in_list(y, LINE_H(PX_MICRO), list_bottom))
            font_text_tracked("INSTALLED - NOT IN CONFIG", r.x + PANEL_PAD, y,
                              PX_MICRO, C_CYAN, 1.0f);

        y += GROUP_HEADER_H;

        if (g_editor.orphans_unverified) {
            if (visible_in_list(y, PLUGIN_LINE_H, list_bottom))
                font_text("EXCLUSIONS UNVERIFIED", r.x + PANEL_PAD, y,
                          PX_MICRO, C_YELLOW);
        } else {
            if (visible_in_list(y, PLUGIN_LINE_H, list_bottom))
                font_text("NOT LISTED IN THE ACTIVE CONFIG", r.x + PANEL_PAD,
                          y, PX_MICRO, C_DIM);
        }
        y += PLUGIN_LINE_H;

        for (int i = 0; i < g_editor.orphan_count; ++i) {
            if (visible_in_list(y, PLUGIN_LINE_H, list_bottom)) {
                char label[PLUGIN_PATH_MAX + 16];
                if (g_editor.orphan_shadowed[i])
                    snprintf(label, sizeof(label), "SHADOWED  %s",
                             g_editor.orphans[i]);
                else
                    snprintf(label, sizeof(label), "%s", g_editor.orphans[i]);
                font_text(label, r.x + PANEL_PAD, y, PX_MICRO, C_DIM);
            }
            y += PLUGIN_LINE_H;
        }

        if (g_editor.orphan_truncated) {
            if (visible_in_list(y, PLUGIN_LINE_H, list_bottom))
                font_text("MORE FILES NOT SHOWN", r.x + PANEL_PAD, y,
                          PX_MICRO, C_YELLOW);
            y += PLUGIN_LINE_H;
        }
    }

    if (g_editor.truncated) {
        y += GROUP_GAP;
        if (visible_in_list(y, PLUGIN_LINE_H, list_bottom))
            font_text("MORE PLUGINS NOT SHOWN", r.x + PANEL_PAD, y, PX_MICRO,
                      C_YELLOW);
    }
}

static void draw_preview_list(Rect r, float list_bottom)
{
    float y = r.y + ROWS_TOP;
    PluginSection group = PLUGIN_SECTION_COUNT;

    for (int i = 0; i < g_editor.row_count; ++i) {
        const EditorRow *row = &g_editor.rows[i];

        if (row->section != group) {
            if (group != PLUGIN_SECTION_COUNT)
                y += GROUP_GAP;

            group = row->section;

            if (visible_in_list(y, LINE_H(PX_MICRO), list_bottom))
                font_text_tracked(editor_section_text(group), r.x + PANEL_PAD,
                                  y, PX_MICRO, C_CYAN, 1.0f);

            y += GROUP_HEADER_H;
        }

        if (visible_in_list(y, PLUGIN_DESC_DY + PLUGIN_LINE_H, list_bottom))
            draw_preview_row(r, i, y);

        y += PLUGIN_STRIDE;
    }

    int removed = editor_preview_removed_count();

    if (removed > 0) {
        y += GROUP_GAP;

        if (visible_in_list(y, LINE_H(PX_MICRO), list_bottom))
            font_text_tracked("REMOVING", r.x + PANEL_PAD, y, PX_MICRO,
                              C_CYAN, 1.0f);

        y += GROUP_HEADER_H;

        for (int i = 0; i < removed; ++i) {
            if (visible_in_list(y, PLUGIN_LINE_H, list_bottom)) {
                const char *path = editor_preview_removed_path(i);
                const char *label = editor_diff_text(EDITOR_DIFF_REMOVE);
                float right = r.x + r.w - PANEL_PAD;
                float path_x = r.x + PANEL_PAD +
                               FONT_ADVANCE * PX_MICRO + SP_1;
                float label_w = font_width(label, PX_MICRO);

                font_text("-", r.x + PANEL_PAD, y, PX_MICRO, C_YELLOW);
                draw_text_fit(path, path_x, right - label_w - SP_2, y,
                              PX_MICRO, C_WHITE);
                font_text_right(label, right, y, PX_MICRO, C_YELLOW);
            }
            y += PLUGIN_LINE_H;
        }
    }
}

static float button_width(PluginFlowState state, PluginDockAction action)
{
    return font_width(plugin_flow_action_label(state, action), PX_LABEL) +
           DOCK_BTN_PAD * 2.0f;
}

/*
 * A pressable slab, not a text chip: a fill, a light top edge, a darker bottom
 * ledge, and a centred label. The selected action gets a white outline offset
 * from the slab; an unavailable action keeps the shape but loses the bevel.
 */
static void draw_button(PluginFlowState state, PluginDockAction action, float x,
                        float y, float h, int primary, int selected)
{
    const char *label = plugin_flow_action_label(state, action);
    int available = plugin_flow_action_available(state, action);
    float w = button_width(state, action);
    float text_y = y + (h - LINE_H(PX_LABEL)) * 0.5f;

    if (selected) {
        draw_rect(x - 4.0f, y - 4.0f, w + 8.0f, 2.0f, C_WHITE);
        draw_rect(x - 4.0f, y + h + 2.0f, w + 8.0f, 2.0f, C_WHITE);
        draw_rect(x - 4.0f, y - 4.0f, 2.0f, h + 8.0f, C_WHITE);
        draw_rect(x + w + 2.0f, y - 4.0f, 2.0f, h + 8.0f, C_WHITE);
    }

    if (!available) {
        draw_rect(x, y, w, h, draw_dim(C_HAIR, 0.35f));
        draw_rect(x, y, w, 1.0f, draw_dim(C_DIM, 0.25f));
        draw_rect(x, y + h - 1.0f, w, 1.0f, draw_dim(C_DIM, 0.25f));
    } else if (primary) {
        draw_rect(x, y, w, h, C_YELLOW);
        draw_rect(x, y, w, 1.0f, draw_dim(C_WHITE, 0.80f));
        draw_rect(x, y + h - DOCK_BTN_LEDGE, w, DOCK_BTN_LEDGE,
                  draw_dim(C_YELLOW, 0.40f));
    } else {
        draw_rect(x, y, w, h, draw_dim(C_HAIR, 0.95f));
        draw_rect(x, y, w, 1.0f, draw_dim(C_WHITE, 0.20f));
        draw_rect(x, y + h - 2.0f, w, 2.0f, C_FIELD);
        draw_rect(x, y, 1.0f, h, draw_dim(C_DIM, 0.55f));
        draw_rect(x + w - 1.0f, y, 1.0f, h, draw_dim(C_DIM, 0.55f));
    }

    uint32_t text = !available ? draw_dim(C_DIM, 0.55f)
                              : (primary ? C_FIELD : C_WHITE);
    float text_w = font_width(label, PX_LABEL);

    font_text(label, x + (w - text_w) * 0.5f, text_y, PX_LABEL, text);
}

static void draw_action_row(const UiState *ui, PluginFlowState state, Rect d,
                            int first, int count, float y, float h)
{
    int selected = ui->focus == UI_FOCUS_DOCK ? ui->dock_choice : -1;
    PluginDockAction primary = plugin_flow_primary_action(state);
    float x = d.x + PANEL_PAD;
    float right = d.x + d.w - PANEL_PAD;

    for (int i = first; i < count; ++i) {
        PluginDockAction action = plugin_flow_action_at(state, i);
        float w = button_width(state, action);

        if (x + w > right)
            break;

        draw_button(state, action, x, y, h, action == primary, i == selected);
        x += w + DOCK_BTN_GAP;
    }
}

static void draw_draft_group(float *x, float y, float right, int change,
                             int total, const char *label,
                             uint32_t count_colour, uint32_t name_colour)
{
    if (total == 0)
        return;

    char number[16];
    snprintf(number, sizeof(number), "%d", total);

    *x = draw_token(*x, y, label, C_DIM) + SP_1;
    *x = draw_token(*x, y, number, count_colour) + SP_2;

    int shown = 0;

    for (int i = 0; i < g_editor.row_count; ++i) {
        if (g_editor.rows[i].draft_change != change)
            continue;

        const char *name = editor_row_label(i);
        float need = font_width(name, PX_MICRO);

        if (shown > 0)
            need += font_width(", ", PX_MICRO);

        int more_after = total - (shown + 1);
        float tail = 0.0f;

        if (more_after > 0) {
            char reserve[24];
            snprintf(reserve, sizeof(reserve), "%s+%d MORE",
                     shown > 0 ? ", " : "", more_after);
            tail = font_width(reserve, PX_MICRO);
        }

        if (*x + need + tail > right)
            break;

        if (shown > 0) {
            font_text(", ", *x, y, PX_MICRO, name_colour);
            *x += font_width(", ", PX_MICRO);
        }

        font_text(name, *x, y, PX_MICRO, name_colour);
        *x += font_width(name, PX_MICRO);
        ++shown;
    }

    int more = total - shown;

    if (more > 0) {
        char tail[24];
        snprintf(tail, sizeof(tail), "%s+%d MORE", shown > 0 ? ", " : "",
                 more);

        draw_text_fit(tail, *x, right, y, PX_MICRO, C_DIM);
        *x += font_width(tail, PX_MICRO);
    }
}

static void draw_draft_summary(Rect d, float y)
{
    int enabling = g_editor.draft_on_total;
    int disabling = g_editor.draft_off_total;
    float left = d.x + PANEL_PAD;
    float right = d.x + d.w - PANEL_PAD;
    float middle = left + (right - left) * 0.5f;
    float x = left;

    if (enabling > 0)
        draw_draft_group(&x, y, disabling > 0 ? middle - SP_2 : right, 1,
                         enabling, "ENABLING", C_GREEN, C_GREEN);

    if (disabling > 0) {
        x = enabling > 0 ? middle : left;
        draw_draft_group(&x, y, right, -1, disabling, "DISABLING", C_RED,
                         C_WHITE);
    }
}

static void draw_preview_counts(Rect d, float y)
{
    int enabling = g_editor.preview_enable_total;
    int disabling = g_editor.preview_disable_total;
    int removing = editor_preview_removed_count();
    float x = d.x + PANEL_PAD;
    char number[16];

    if (enabling > 0) {
        snprintf(number, sizeof(number), "%d", enabling);
        x = draw_token(x, y, "ENABLING", C_DIM) + SP_1;
        x = draw_token(x, y, number, C_GREEN) + DOCK_SEGMENT_GAP;
    }

    if (disabling > 0) {
        snprintf(number, sizeof(number), "%d", disabling);
        x = draw_token(x, y, "DISABLING", C_DIM) + SP_1;
        x = draw_token(x, y, number, C_RED) + DOCK_SEGMENT_GAP;
    }

    if (removing > 0) {
        snprintf(number, sizeof(number), "%d", removing);
        x = draw_token(x, y, "REMOVING", C_DIM) + SP_1;
        draw_token(x, y, number, C_YELLOW);
    }
}

static void draw_dock_title(Rect d, float y, const char *title)
{
    if (title == NULL)
        return;

    font_text(title, d.x + PANEL_PAD, y, PX_LABEL, C_YELLOW);
}

static void draw_dock_rule(Rect d, float y)
{
    draw_rect(d.x + PANEL_PAD, y, d.w - 2.0f * PANEL_PAD, 1.0f, C_HAIR);
}

static void draw_dock_warning_line(const UiState *ui, Rect d, float y)
{
    char warning[TEXT_FIT_MAX];
    plugin_flow_dock_warning(ui, warning, sizeof(warning));

    if (warning[0] == '\0')
        return;

    float left = d.x + PANEL_PAD;

    draw_rect(left, y - 1.0f, 2.0f, LINE_H(PX_MICRO) + 2.0f, C_YELLOW);
    draw_text_fit(warning, left + SP_2, d.x + d.w - PANEL_PAD, y, PX_MICRO,
                  C_YELLOW);
}

static void draw_dock(const UiState *ui)
{
    PluginFlowState state = plugin_flow_state();
    int h = (int)plugins_dock_height();
    float top = plugins_list_bottom();
    Rect d = {(float)MARGIN, top, (float)(SCREEN_W - 2 * MARGIN), (float)h};

    draw_rect(d.x, d.y, d.w, d.h, C_PANEL);
    draw_rect(d.x, d.y, d.w, 1.0f, C_HAIR);
    draw_rect(d.x, d.y + d.h - 1.0f, d.w, 1.0f, C_HAIR);
    draw_rect(d.x, d.y, 1.0f, d.h, C_HAIR);
    draw_rect(d.x + d.w - 1.0f, d.y, 1.0f, d.h, C_HAIR);

    if (state == PLUGIN_FLOW_DRAFT || state == PLUGIN_FLOW_PREVIEW)
        draw_rect(d.x, d.y, 2.0f, d.h, C_YELLOW);

    int count = plugin_flow_action_count(state);
    int primary_count = plugin_flow_primary_count(state);

    if (state == PLUGIN_FLOW_IDLE) {
        draw_action_row(ui, state, d, 0, count, d.y + DOCK_IDLE_ACTION_DY,
                        DOCK_BTN_H);

        /*
         * A failed restore leaves IDLE unchanged, so its reason is drawn beside
         * the still-available buttons rather than dropped.
         */
        char warning[TEXT_FIT_MAX];
        plugin_flow_dock_warning(ui, warning, sizeof(warning));

        if (warning[0] != '\0') {
            float warn_x = d.x + d.w * 0.60f;

            draw_rect(warn_x - SP_2,
                      d.y + DOCK_IDLE_ACTION_DY + 9.0f, 2.0f,
                      LINE_H(PX_MICRO) + 2.0f, C_YELLOW);
            draw_text_fit(warning, warn_x, d.x + d.w - PANEL_PAD,
                          d.y + DOCK_IDLE_ACTION_DY + 10.0f, PX_MICRO,
                          C_YELLOW);
        }

        return;
    }

    if (state == PLUGIN_FLOW_DRAFT) {
        draw_dock_title(d, d.y + DOCK_TITLE_DY,
                        plugin_flow_state_title(state));
        draw_draft_summary(d, d.y + DOCK_SUMMARY_DY);
        draw_dock_rule(d, d.y + DOCK_RULE_DY);
        draw_action_row(ui, state, d, 0, primary_count,
                        d.y + DOCK_ACTION_DY, DOCK_BTN_H);
        draw_action_row(ui, state, d, primary_count, count,
                        d.y + DOCK_SECONDARY_DY, DOCK_BTN_SMALL_H);
        draw_dock_warning_line(ui, d, d.y + DOCK_WARNING_DY);
        return;
    }

    if (state == PLUGIN_FLOW_PREVIEW) {
        draw_dock_title(d, d.y + DOCK_TITLE_DY, plugin_flow_preview_title());
        draw_preview_counts(d, d.y + DOCK_SUMMARY_DY);
        draw_dock_rule(d, d.y + DOCK_RULE_DY);
        draw_action_row(ui, state, d, 0, count,
                        d.y + DOCK_PREVIEW_ACTION_DY, DOCK_BTN_H);
        draw_dock_warning_line(ui, d, d.y + DOCK_PREVIEW_WARNING_DY);
        return;
    }

    draw_dock_title(d, d.y + DOCK_SAVED_TITLE_DY,
                    plugin_flow_state_title(state));
    draw_dock_rule(d, d.y + DOCK_SAVED_RULE_DY);
    draw_action_row(ui, state, d, 0, count, d.y + DOCK_SAVED_ACTION_DY,
                    DOCK_BTN_H);
    draw_dock_warning_line(ui, d, d.y + DOCK_SAVED_WARNING_DY);
}

void plugins_screen_draw(const UiState *ui, float content_h)
{
    if (ui == NULL)
        return;

    float list_bottom = plugins_list_bottom();
    float min_h = (float)(PLUGINS_VIEW_H - VIEW_TOP_GAP);
    float panel_h = content_h > min_h ? content_h : min_h;
    int max_scroll = plugins_max_scroll();
    float scroll = ui->plugins_scroll;

    /*
     * The dock can grow after a stage, which can leave the stored offset past
     * the new range. Clamp for drawing only; the app owns the state and will
     * settle it on the next input (U4).
     */
    if (scroll < 0.0f)
        scroll = 0.0f;
    if (scroll > (float)max_scroll)
        scroll = (float)max_scroll;

    Rect r = {(float)MARGIN,
              (float)(VIEW_Y + VIEW_TOP_GAP) - scroll,
              (float)(SCREEN_W - 2 * MARGIN), panel_h};

    draw_set_clip(VIEW_X, VIEW_Y, VIEW_W,
                  (int)plugins_list_bottom() - VIEW_Y);
    theme_panel(r, "PLUGINS", C_CYAN, CHROME_LIST);

    draw_header_lines(r);

    if (editor_preview_active())
        draw_preview_list(r, list_bottom);
    else
        draw_edit_list(ui, r, list_bottom);

    draw_clear_clip();
    draw_dock(ui);
}
