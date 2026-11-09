#include "ui/core_text.h"

#include <stdio.h>

#include "gfx/theme.h"

void core_text_format_core(char *buf, size_t n, int pct, int valid)
{
    if (buf == NULL || n == 0)
        return;

    if (!valid)
        snprintf(buf, n, "%s", THEME_ABSENT);
    else
        snprintf(buf, n, "%d%%", pct);
}

void core_text_format_caption(char *left, size_t left_n,
                              char *right, size_t right_n,
                              int cpu_valid, int usage_pct,
                              int ctx_valid, int ctx_per_s,
                              int wake_valid, int wake_per_s)
{
    char ctx[16];
    char wake[16];

    if (left != NULL && left_n > 0) {
        if (cpu_valid)
            snprintf(left, left_n, "CPU LOAD AVG %d%%", usage_pct);
        else
            snprintf(left, left_n, "CPU LOAD AVG %s", THEME_ABSENT);
    }

    if (right == NULL || right_n == 0)
        return;

    if (ctx_valid)
        snprintf(ctx, sizeof(ctx), "%d/S", ctx_per_s);
    else
        snprintf(ctx, sizeof(ctx), "%s", THEME_ABSENT);

    if (wake_valid)
        snprintf(wake, sizeof(wake), "%d/S", wake_per_s);
    else
        snprintf(wake, sizeof(wake), "%s", THEME_ABSENT);

    snprintf(right, right_n, "CTX %s  WAKE %s", ctx, wake);
}
