#include "app/screen.h"

#include "gfx/draw.h"
#include "gfx/layout.h"
#include "gfx/theme.h"
#include "panels/plugins.h"
#include "panels/registry.h"
#include "ui/about.h"
#include "ui/header.h"
#include "ui/modal.h"

/* Thin track on the right edge: without an affordance a scrollable screen
 * looks like a screen that simply ends. */
#define SCROLLBAR_X ((float)SCREEN_W - 8.0f)
#define SCROLLBAR_W 4.0f

static void draw_scrollbar(float scroll, int max_scroll, int total_h,
                           float track_h)
{
    if (max_scroll <= 0 || total_h <= 0 || track_h <= 0.0f)
        return;

    float track_y = (float)(VIEW_Y + VIEW_TOP_GAP);
    float thumb_h = track_h * track_h / (float)total_h;
    float t = scroll / (float)max_scroll;

    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;

    draw_rect(SCROLLBAR_X, track_y, SCROLLBAR_W, track_h, C_HAIR);
    draw_rect(SCROLLBAR_X, track_y + t * (track_h - thumb_h), SCROLLBAR_W,
              thumb_h, C_DIM);
}

static void draw_vitals(const UiState *ui)
{
    draw_set_clip(VIEW_X, VIEW_Y, VIEW_W, VIEW_H);

    for (int i = 0; i < g_layout_count; ++i) {
        const PanelSlot *slot = &g_layout[i];

        Rect r = slot->rect;
        r.y += (float)(VIEW_Y + VIEW_TOP_GAP) - ui->vitals_scroll;

        if (r.y + r.h <= (float)VIEW_Y || r.y >= (float)(VIEW_Y + VIEW_H))
            continue;

        theme_panel(r, slot->panel->title, slot->panel->accent, slot->chrome);
        slot->panel->draw(slot->panel, r);
    }

    draw_clear_clip();
}

void screen_draw(float t, const UiState *ui)
{
    header_draw(ui);

    if (ui->tab == UI_TAB_PLUGINS) {
        float content_h = plugins_content_height();
        float track_h = (float)plugins_viewport_height();

        plugins_screen_draw(ui, content_h);
        draw_scrollbar(ui->plugins_scroll, plugins_max_scroll(),
                       (int)content_h, track_h);
    } else if (ui->tab == UI_TAB_VITALS) {
        draw_vitals(ui);
        draw_scrollbar(ui->vitals_scroll, layout_max_scroll(),
                       layout_canvas_height(),
                       (float)(VIEW_H - VIEW_TOP_GAP));

        /*
         * Frame health belongs to VITALS. PLUGINS uses the full bottom of the
         * screen for its pinned transaction dock (D81).
         */
        theme_panel(g_frame.rect, g_frame.panel->title, g_frame.panel->accent,
                    g_frame.chrome);
        g_frame.panel->draw(g_frame.panel, g_frame.rect);
    } else {
        about_draw(ui);
    }

    modal_draw(ui);

    /* Last, so it reads as a scan over the whole screen. */
    theme_sweep(0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H, t);
}
