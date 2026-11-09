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
#define REBOOT_MAX_LINES 6
/* The glyph box is exactly LINE_H; two pixels of leading keep lines apart. */
#define MODAL_LINE_GAP 2.0f

/*
 * The launch card carries more copy than an error, and its snapshot sentence
 * must never be the line that truncates. It gets its own geometry instead of
 * reusing the 600x240 error box.
 */
#define SAFETY_W 720.0f
#define SAFETY_H 260.0f
#define SAFETY_X ((float)(SCREEN_W / 2) - SAFETY_W / 2.0f)
#define SAFETY_Y 130.0f
#define SAFETY_BODY_Y 54.0f
#define SAFETY_KEY_Y 226.0f
#define SAFETY_BODY_W (SAFETY_W - 2.0f * MODAL_PAD)
#define SAFETY_MAX_LINES 8

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

void modal_open_safety(UiState *ui, const LaunchCard *card)
{
    if (ui == NULL || card == NULL)
        return;

    ui->modal = UI_MODAL_SAFETY;
    copy_text(ui->modal_title, sizeof(ui->modal_title), card->title);
    copy_text(ui->modal_body, sizeof(ui->modal_body), card->body);
}

void modal_open_reboot(UiState *ui, const char *title, const char *text)
{
    if (ui == NULL)
        return;

    ui->modal = UI_MODAL_REBOOT;
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
            font_text(line, x, y + (float)lines * (LINE_H(px) + MODAL_LINE_GAP),
                      px, colour);
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
        font_text(line, x, y + (float)lines * (LINE_H(px) + MODAL_LINE_GAP), px,
                  colour);

    return lines;
}

void modal_draw(const UiState *ui)
{
    if (ui == NULL || ui->modal == UI_MODAL_NONE)
        return;

    theme_scrim(0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H);

    if (ui->modal == UI_MODAL_SAFETY) {
        Rect r = {SAFETY_X, SAFETY_Y, SAFETY_W, SAFETY_H};

        theme_panel(r, ui->modal_title, C_YELLOW, CHROME_SATELLITE);
        draw_wrapped(ui->modal_body, r.x + MODAL_PAD, r.y + SAFETY_BODY_Y,
                     SAFETY_BODY_W, PX_LABEL, C_WHITE, SAFETY_MAX_LINES);
        font_text("CROSS OR CIRCLE CONTINUE", r.x + MODAL_PAD,
                  r.y + SAFETY_KEY_Y, PX_MICRO, C_DIM);
        return;
    }

    Rect r = {MODAL_X, MODAL_Y, MODAL_W, MODAL_H};

    theme_panel(r, ui->modal_title, C_YELLOW, CHROME_SATELLITE);

    if (ui->modal == UI_MODAL_REBOOT)
        draw_wrapped(ui->modal_body, r.x + MODAL_PAD, r.y + MODAL_BODY_Y,
                     MODAL_BODY_W, PX_LABEL, C_WHITE, REBOOT_MAX_LINES);
    else
        draw_wrapped(ui->modal_body, r.x + MODAL_PAD, r.y + MODAL_BODY_Y,
                     MODAL_BODY_W, PX_MICRO, C_WHITE, MODAL_MAX_LINES);

    font_text(ui->modal == UI_MODAL_REBOOT ? "CROSS CONFIRM   CIRCLE CANCEL"
                                           : "CROSS CLOSE",
              r.x + MODAL_PAD, r.y + MODAL_KEY_Y, PX_MICRO, C_DIM);
}
