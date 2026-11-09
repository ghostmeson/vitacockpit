#include "ui/modal.h"

#include <stdio.h>
#include <string.h>

#include "gfx/font.h"
#include "gfx/layout.h"
#include "gfx/theme.h"

#define MODAL_W 600.0f
#define MODAL_H 240.0f
#define MODAL_X ((float)(SCREEN_W / 2) - MODAL_W / 2.0f)
#define MODAL_Y 152.0f
#define MODAL_PAD 24.0f
#define MODAL_BODY_Y 54.0f
#define MODAL_KEY_Y 206.0f
#define MODAL_BODY_W (MODAL_W - 2.0f * MODAL_PAD)
#define MODAL_MAX_LINES 10

static void copy_text(char *dst, size_t cap, const char *src)
{
    if (cap == 0)
        return;

    snprintf(dst, cap, "%s", src == NULL ? "" : src);
}

void modal_open_error_titled(UiState *ui, const char *title, const char *text)
{
    if (ui == NULL)
        return;

    ui->modal = UI_MODAL_ERROR;
    copy_text(ui->modal_title, sizeof(ui->modal_title), title);
    copy_text(ui->modal_body, sizeof(ui->modal_body), text);
}

static int draw_wrapped(const char *text, float x, float y, float max_w,
                        float px, uint32_t colour, int max_lines)
{
    char line[256];
    int used = 0;
    int lines = 0;
    const char *p = text;

    if (text == NULL || *text == '\0')
        return 0;

    line[0] = '\0';

    while (*p != '\0' && lines < max_lines) {
        while (*p == ' ' || *p == '\n')
            ++p;
        if (*p == '\0')
            break;

        const char *word = p;
        while (*p != '\0' && *p != ' ' && *p != '\n')
            ++p;

        int word_len = (int)(p - word);
        char candidate[256];

        if (used > 0)
            snprintf(candidate, sizeof(candidate), "%s %.*s", line, word_len,
                     word);
        else
            snprintf(candidate, sizeof(candidate), "%.*s", word_len, word);

        if (used > 0 && font_width(candidate, px) > max_w) {
            font_text(line, x, y + (float)lines * LINE_H(px), px, colour);
            ++lines;
            if (lines >= max_lines)
                break;

            snprintf(line, sizeof(line), "%.*s", word_len, word);
            used = (int)strlen(line);
        } else {
            snprintf(line, sizeof(line), "%s", candidate);
            used = (int)strlen(line);
        }
    }

    if (lines < max_lines && line[0] != '\0')
        font_text(line, x, y + (float)lines * LINE_H(px), px, colour);

    return lines;
}

void modal_draw(const UiState *ui)
{
    if (ui == NULL || ui->modal == UI_MODAL_NONE)
        return;

    Rect r = {MODAL_X, MODAL_Y, MODAL_W, MODAL_H};

    theme_panel(r, ui->modal_title, C_YELLOW, CHROME_SATELLITE);

    draw_wrapped(ui->modal_body, r.x + MODAL_PAD, r.y + MODAL_BODY_Y,
                 MODAL_BODY_W, PX_MICRO, C_WHITE, MODAL_MAX_LINES);

    font_text("CROSS CLOSE", r.x + MODAL_PAD, r.y + MODAL_KEY_Y, PX_MICRO,
              C_DIM);
}
