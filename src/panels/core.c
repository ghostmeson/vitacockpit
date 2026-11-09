#include "panels/panel.h"

#include "core/metrics.h"
#include "gfx/font.h"
#include "ui/core_text.h"

/*
 * CORE — the hero. It reports what the machine is doing and how much memory is
 * left.
 *
 * CPU load is system-wide, sampled from the scheduler's per-core idle clocks
 * through sceKernelGetSystemInfo() in the platform adapter. It is not this
 * process's share. The four cores are shown individually and averaged into the
 * trace, so a single busy core cannot hide behind an idle machine.
 *
 * The applied ARM/GPU/BUS clocks are still unavailable without a kernel or
 * bridge source; the userland ScePower getters return this process's request,
 * not the machine's value. They are deliberately not printed. GPU work shows up
 * as the CDRAM/PHY pools here and as frame time in the pinned FRAME strip.
 */

#define CORE_CPU_COL_W 106.0f
#define CORE_CPU_COL_GAP 20.0f

#define CORE_MEM_COL_W 120.0f
#define CORE_MEM_COL_GAP 62.0f

#define CORE_CPU_Y PANEL_BODY_Y
#define CORE_MEM_Y (CORE_CPU_Y + READOUT_STRIDE)
#define CORE_CAPTION_Y (CORE_MEM_Y + LINE_H(PX_VALUE) + SP_4)
#define CORE_TRACE_Y (CORE_CAPTION_Y + LINE_H(PX_MICRO) + SP_2)
#define CORE_TRACE_H 52.0f

#define NUM_BUF 24

static float cpu_col_x(Rect r, int i)
{
    return r.x + PANEL_INSET +
           (float)i * (CORE_CPU_COL_W + CORE_CPU_COL_GAP);
}

static float mem_col_x(Rect r, int i)
{
    return r.x + PANEL_INSET +
           (float)i * (CORE_MEM_COL_W + CORE_MEM_COL_GAP);
}

static void draw_cpu_row(Rect r)
{
    static const char *const LABELS[CPU_CORE_COUNT] = {"C0", "C1", "C2", "C3"};
    char value[NUM_BUF];

    for (int i = 0; i < CPU_CORE_COUNT; ++i) {
        int pct = g_metrics.cpu_core_pct[i];
        int valid = pct != CPU_CORE_UNKNOWN;

        core_text_format_core(value, sizeof(value), pct, valid);
        theme_readout_px(LABELS[i], value, valid, cpu_col_x(r, i),
                         r.y + CORE_CPU_Y, CORE_CPU_COL_W, PX_LABEL,
                         PX_VALUE);
    }
}

static void draw_memory_row(Rect r)
{
    static const char *const LABELS[3] = {"USER", "CDRAM", "PHY"};
    int mb[3] = {g_metrics.mem_user_mb, g_metrics.mem_cdram_mb,
                 g_metrics.mem_phy_mb};

    for (int i = 0; i < 3; ++i)
        theme_readout_int(LABELS[i], mb[i], g_metrics.mem_valid,
                          mem_col_x(r, i), r.y + CORE_MEM_Y,
                          CORE_MEM_COL_W);
}

static void draw_caption(Rect r)
{
    float x = r.x + PANEL_INSET;
    float w = r.w - 2.0f * PANEL_INSET;
    char left[NUM_BUF];
    char right[48];

    core_text_format_caption(left, sizeof(left), right, sizeof(right),
                             g_metrics.cpu_valid, g_metrics.cpu_usage_pct,
                             g_metrics.cpu_ctx_valid, g_metrics.cpu_ctx_per_s,
                             g_metrics.cpu_wake_valid,
                             g_metrics.cpu_wake_per_s);

    font_text(left, x, r.y + CORE_CAPTION_Y, PX_MICRO, C_DIM);
    font_text_right(right, x + w, r.y + CORE_CAPTION_Y, PX_MICRO, C_DIM);
}

static void draw_trace(Rect r)
{
    float x = r.x + PANEL_INSET;
    float w = r.w - 2.0f * PANEL_INSET;

    /*
     * A fixed 0-100 % axis, unlike the old free-memory trace. A CPU load line
     * that autoscaled would make 5 % look as busy as 95 %.
     */
    if (!g_metrics.cpu_valid || g_metrics.hist_cpu.len <= 0)
        return;

    theme_sparkline(&g_metrics.hist_cpu, x, r.y + CORE_TRACE_Y, w,
                    CORE_TRACE_H, C_YELLOW, 0.0f, 100.0f);
}

static void draw_core(const Panel *self, Rect r)
{
    (void)self;

    draw_cpu_row(r);
    draw_memory_row(r);
    draw_caption(r);
    draw_trace(r);
}

const Panel panel_core = {"CORE", C_YELLOW, draw_core};
