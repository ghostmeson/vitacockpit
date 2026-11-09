#include "ui/about.h"

#include <stdio.h>

#include "cockpit_build.h"
#include "gfx/font.h"
#include "gfx/layout.h"
#include "gfx/theme.h"
#include "ui/plugin_flow.h"

/*
 * ABOUT is the page that identifies the build and offers the one console
 * action. It is deliberately a single panel: the app already has a wordmark in
 * the header, so this page names the product at label scale and spends the
 * space on the version, the reboot state, and the real power-off path.
 */

#define ABOUT_Y 76.0f
#define ABOUT_H 370.0f
#define ABOUT_TITLE_Y (ABOUT_Y + PANEL_BODY_Y)
#define ABOUT_DESC_Y (ABOUT_TITLE_Y + LINE_H(PX_HERO) + SP_3)
#define ABOUT_DESC_STEP (LINE_H(PX_MICRO) + SP_1)
#define ABOUT_VERSION_Y (ABOUT_DESC_Y + 2.0f * ABOUT_DESC_STEP + SP_5)
#define ABOUT_REBOOT_Y (ABOUT_VERSION_Y + READOUT_STRIDE + SP_5)
#define ABOUT_REBOOT_W 320.0f
#define ABOUT_REBOOT_H 48.0f
#define ABOUT_REASON_Y (ABOUT_REBOOT_Y + ABOUT_REBOOT_H + SP_4)
#define ABOUT_POWER_Y (ABOUT_REASON_Y + LINE_H(PX_MICRO) + SP_4)
#define ABOUT_POWER_STEP (LINE_H(PX_MICRO) + SP_1)

static void draw_reboot(const char *block)
{
    float x = (float)MARGIN + PANEL_INSET;
    float y = ABOUT_REBOOT_Y;
    int allowed = block == NULL;

    draw_rect(x, y, ABOUT_REBOOT_W, ABOUT_REBOOT_H, C_HAIR);

    if (allowed) {
        /* The same 2 px white outline the dock gives its focused action. */
        draw_rect(x - 4.0f, y - 4.0f, ABOUT_REBOOT_W + 8.0f, 2.0f, C_WHITE);
        draw_rect(x - 4.0f, y + ABOUT_REBOOT_H + 2.0f,
                  ABOUT_REBOOT_W + 8.0f, 2.0f, C_WHITE);
        draw_rect(x - 4.0f, y - 4.0f, 2.0f, ABOUT_REBOOT_H + 8.0f, C_WHITE);
        draw_rect(x + ABOUT_REBOOT_W + 2.0f, y - 4.0f, 2.0f,
                  ABOUT_REBOOT_H + 8.0f, C_WHITE);
    }

    font_text("REBOOT", x + SP_4, y + 17.0f, PX_LABEL,
              allowed ? C_WHITE : C_DIM);

    if (allowed)
        font_text_right("CROSS", x + ABOUT_REBOOT_W - SP_4, y + 19.0f,
                        PX_MICRO, C_DIM);
}

void about_draw(void)
{
    Rect r = {(float)MARGIN, ABOUT_Y, (float)(SCREEN_W - 2 * MARGIN), ABOUT_H};
    const char *block = plugin_flow_reboot_block_reason();

    theme_panel(r, "ABOUT", C_CYAN, CHROME_SATELLITE);

    float x = r.x + PANEL_INSET;

    font_text_tracked("COCKPIT", x, ABOUT_TITLE_Y, PX_HERO, C_WHITE, 1.0f);

    font_text("TAIHEN CONFIG.TXT MANAGER. ENABLE AND DISABLE PLUGINS",
              x, ABOUT_DESC_Y, PX_MICRO, C_WHITE);
    font_text("WITHOUT A MANUAL EDIT. VITALS SHOWS THE MACHINE.",
              x, ABOUT_DESC_Y + ABOUT_DESC_STEP, PX_MICRO, C_WHITE);

    theme_readout("VERSION", COCKPIT_RELEASE_VERSION, 1, x, ABOUT_VERSION_Y,
                  240.0f);
    theme_readout("BUILD", COCKPIT_BUILD_NUMBER, 1, x + 280.0f,
                  ABOUT_VERSION_Y, 240.0f);

    draw_reboot(block);

    if (block != NULL)
        font_text(block, x, ABOUT_REASON_Y, PX_MICRO, C_YELLOW);

    font_text("POWER OFF: HOLD THE POWER BUTTON 2 SEC, THEN CHOOSE POWER OFF.",
              x, ABOUT_POWER_Y, PX_MICRO, C_DIM);
    font_text("IF THE SCREEN IS FROZEN, HOLD 5 SEC FOR A FORCED SHUTDOWN.",
              x, ABOUT_POWER_Y + ABOUT_POWER_STEP, PX_MICRO, C_DIM);
}
