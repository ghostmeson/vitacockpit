#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "app/screen.h"
#include "ui/ui_state.h"
#include "core/history.h"
#include "core/metrics.h"
#include "core/plugin_editor.h"
#include "core/platform.h"
#include "core/plugins.h"
#include "gfx/layout.h"
#include "panels/plugins.h"
#include "panels/registry.h"
#include "ui/about_text.h"
#include "ui/launch_card.h"
#include "ui/modal.h"
#include "ui/plugin_flow.h"
#include "fake_platform.h"

/*
 * Host preview renderer.
 *
 * Draws the real screen through the shipped composition (app/screen.c), the
 * real theme, panels and header, a software vita2d shim, and the shared fake
 * platform. It is the only way to look at the layout without a Vita, and it is
 * deliberately outside the app: none of this is linked into cockpit.self.
 *
 * Scenes are produced by driving the fake port and the real sampler, never by
 * writing g_metrics directly -- so a PNG here proves the policy ran (D26).
 *
 * Run with tools/preview/run.sh.
 */

extern void preview_clear(void);
extern const uint32_t *preview_pixels(void);
extern int preview_width(void);
extern int preview_height(void);
extern int preview_draw_calls(void);

/*
 * Ceiling on one frame's draw calls. The renderer deliberately issues one call
 * per rect (D31), so a full frame with the hazard band up is ~3,000. The budget
 * catches accidental growth, not the batching that was removed.
 */
#define DRAW_CALL_BUDGET 8000

/* 200 slow samples at 10 Hz fills the 20 s window the trace claims. */
static void seed_trace(void)
{
    for (int i = 0; i < HISTORY_CAP; ++i) {
        float ph = (float)i * 0.06f;
        int mem = 182 + (int)(9.0f * sinf(ph) + 3.0f * sinf(ph * 2.7f));
        float ms = 16.6f + 1.6f * sinf(ph * 1.9f);

        if (i % 47 == 0)
            ms += 9.0f;

        fake_set_memory_mb(mem);
        fake_set_cpu_usage(0, 18 + (int)(8.0f * sinf(ph)));
        fake_set_cpu_usage(1, 22 + (int)(10.0f * sinf(ph * 1.3f + 1.0f)));
        fake_set_cpu_usage(2, 9 + (int)(5.0f * sinf(ph * 0.7f)));
        fake_set_cpu_usage(3, 4 + (int)(3.0f * sinf(ph * 1.7f)));
        metrics_sample_frame(ms / 1000.0f);
        metrics_sample_slow();
    }
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void png_chunk(FILE *f, const char *type, const uint8_t *data,
                      uint32_t len)
{
    uint8_t hdr[8];
    be32(hdr, len);
    memcpy(hdr + 4, type, 4);
    fwrite(hdr, 1, sizeof(hdr), f);
    if (len > 0)
        fwrite(data, 1, len, f);

    uint32_t c = (uint32_t)crc32(0L, Z_NULL, 0);
    c = (uint32_t)crc32(c, (const Bytef *)type, 4);
    if (len > 0)
        c = (uint32_t)crc32(c, data, len);

    uint8_t cb[4];
    be32(cb, c);
    fwrite(cb, 1, sizeof(cb), f);
}

static int write_png(const char *path)
{
    int w = preview_width();
    int h = preview_height();
    const uint32_t *fb = preview_pixels();

    size_t stride = 1 + (size_t)w * 3;
    size_t raw_len = (size_t)h * stride;
    uint8_t *raw = malloc(raw_len);
    if (raw == NULL)
        return 0;

    for (int y = 0; y < h; ++y) {
        uint8_t *row = raw + (size_t)y * stride;
        row[0] = 0; /* filter: none */
        for (int x = 0; x < w; ++x) {
            uint32_t c = fb[(size_t)y * (size_t)w + (size_t)x];
            row[1 + x * 3 + 0] = (uint8_t)(c & 0xFF);
            row[1 + x * 3 + 1] = (uint8_t)((c >> 8) & 0xFF);
            row[1 + x * 3 + 2] = (uint8_t)((c >> 16) & 0xFF);
        }
    }

    uLongf comp_len = compressBound((uLong)raw_len);
    uint8_t *comp = malloc(comp_len);
    if (comp == NULL) {
        free(raw);
        return 0;
    }

    if (compress2(comp, &comp_len, raw, (uLong)raw_len, 9) != Z_OK) {
        free(raw);
        free(comp);
        return 0;
    }

    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        free(raw);
        free(comp);
        return 0;
    }

    static const uint8_t SIG[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(SIG, 1, sizeof(SIG), f);

    uint8_t ihdr[13];
    be32(ihdr, (uint32_t)w);
    be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 2;  /* truecolour */
    ihdr[10] = 0; /* deflate */
    ihdr[11] = 0; /* adaptive filtering */
    ihdr[12] = 0; /* no interlace */

    png_chunk(f, "IHDR", ihdr, sizeof(ihdr));
    png_chunk(f, "IDAT", comp, (uint32_t)comp_len);
    png_chunk(f, "IEND", NULL, 0);

    fclose(f);
    free(raw);
    free(comp);
    return 1;
}

static int render(const char *dir, const char *name, float t,
                  const UiState *ui)
{
    char path[1024];

    preview_clear();
    screen_draw(t, ui);

    int calls = preview_draw_calls();

    printf("%-22s %5d draw calls\n", name, calls);

    if (calls > DRAW_CALL_BUDGET) {
        printf("FAIL %s: %d draw calls exceeds budget %d\n", name, calls,
               DRAW_CALL_BUDGET);
        return 0;
    }

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (!write_png(path))
        return 0;

    printf("wrote %s\n", path);
    return 1;
}

static void reset_scene(UiState *ui)
{
    fake_reset();
    metrics_init();
    seed_trace();
    editor_scan();
    plugins_scan();

    memset(ui, 0, sizeof(*ui));
    ui->tab = UI_TAB_VITALS;
    ui->selection = g_editor.row_count > 0 ? 0 : -1;
}

static void make_long_config(void)
{
    char cfg[EDITOR_LINES_MAX * 24 + 64];
    size_t off = 0;

    off += (size_t)snprintf(cfg + off, sizeof(cfg) - off, "*KERNEL\n");
    for (int i = 0; i < EDITOR_LINES_MAX + 1; ++i)
        off += (size_t)snprintf(cfg + off, sizeof(cfg) - off,
                                "ur0:tai/p%03d.skprx\n", i);

    fake_add_file("ur0:tai/config.txt", cfg);
}

static void open_plugins(UiState *ui)
{
    ui->tab = UI_TAB_PLUGINS;
    ui->selection = g_editor.row_count > 0 ? 0 : -1;
}

static void add_row_states_config(void)
{
    fake_add_file("ur0:tai/config.txt",
                  "*KERNEL\n"
                  "ur0:tai/kubridge.skprx\n"
                  "ur0:tai/storagemgr.skprx\n"
                  "ur0:tai/taihen.skprx\n"
                  "#ur0:tai/mystery.suprx\n"
                  "ur0:tai/absent.skprx\n"
                  "*main\n"
                  "ur0:tai/henkaku.suprx\n"
                  "#ur0:tai/storagemgr.skprx\n");
    fake_add_file("ur0:tai/taihen.skprx", "");
}

static void add_row_locked_config(void)
{
    fake_add_file("ur0:tai/config.txt",
                  "*main\n"
                  "ur0:tai/henkaku.suprx\n"
                  "ur0:tai/mystery.suprx\n"
                  "*!ALL\n"
                  "ur0:tai/extra.skprx\n");
}

/* A compact working set and a snapshot that exercises every diff direction. */
static const char PREVIEW_BASE_CONFIG[] =
    "*KERNEL\n"
    "ur0:tai/kubridge.skprx\n"
    "#ur0:tai/shellbat.suprx\n"
    "*main\n"
    "ur0:tai/yamt.suprx\n"
    "ur0:tai/henkaku.suprx\n";

static const char PREVIEW_SNAPSHOT[] =
    "*KERNEL\n"
    "#ur0:tai/kubridge.skprx\n"
    "ur0:tai/extra.skprx\n"
    "*main\n"
    "ur0:tai/shellbat.suprx\n"
    "ur0:tai/henkaku.suprx\n";

static const char PREVIEW_SESSION_NO_HENKAKU[] =
    "# sample tai config\n"
    "*KERNEL\n"
    "ur0:tai/kubridge.skprx\n"
    "ur0:tai/storagemgr.skprx\n"
    "ur0:tai/0syscall6.skprx\n"
    "*main\n"
    "ur0:tai/libshacccg.suprx\n"
    "#ur0:tai/shellbat.suprx\n"
    "#ur0:tai/mystery.suprx\n"
    "*NPXS10015\n"
    "ur0:tai/yamt.suprx\n"
    "*ALL\n"
    "ur0:tai/vita3g-disable.suprx\n";

static const char PREVIEW_SESSION_OFF_KUBRIDGE[] =
    "# sample tai config\n"
    "*KERNEL\n"
    "#ur0:tai/kubridge.skprx\n"
    "ur0:tai/storagemgr.skprx\n"
    "ur0:tai/0syscall6.skprx\n"
    "*main\n"
    "ur0:tai/henkaku.suprx\n"
    "ur0:tai/libshacccg.suprx\n"
    "#ur0:tai/shellbat.suprx\n"
    "#ur0:tai/mystery.suprx\n"
    "*NPXS10015\n"
    "ur0:tai/yamt.suprx\n"
    "*ALL\n"
    "ur0:tai/vita3g-disable.suprx\n";

static void stage_batch(UiState *ui)
{
    char err[96];
    int kernel = editor_row_for_path("ur0:tai/kubridge.skprx");
    int shell = editor_row_for_path("ur0:tai/shellbat.suprx");

    if (kernel >= 0)
        (void)editor_stage_toggle(kernel, 0, err, (int)sizeof(err));
    if (shell >= 0)
        (void)editor_stage_toggle(shell, 1, err, (int)sizeof(err));

    open_plugins(ui);
}

/* One stage and one commit, the pair the dock performs for a saved change. */
static void stage_commit(int row, int mode)
{
    char err[96];

    if (row < 0)
        return;

    if (editor_stage_toggle(row, mode, err, (int)sizeof(err)) ==
        EDITOR_WRITE_OK)
        (void)editor_commit_draft(err, (int)sizeof(err));
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    UiState ui;
    char err[96];
    char title[32];
    char body[256];

    reset_scene(&ui);

    if (!render(dir, "preview-healthy.png", 2.5f, &ui))
        return 1;

    /* The launch safety card is the first screen of the shipped app. */
    LaunchCard card;
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    launch_card_build(&card);
    modal_open_safety(&ui, &card);
    if (!render(dir, "preview-safety-card.png", 2.6f, &ui))
        return 1;
    ui.modal = UI_MODAL_NONE;

    /* A failed snapshot write: the card must not claim a restore point. */
    reset_scene(&ui);
    fake_fail_backup(1);
    editor_scan();
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    launch_card_build(&card);
    modal_open_safety(&ui, &card);
    if (!render(dir, "preview-safety-card-failed.png", 2.61f, &ui))
        return 1;
    ui.modal = UI_MODAL_NONE;

    /* No config at all: locked, and no restore is promised. */
    reset_scene(&ui);
    fake_remove_file("ur0:tai/config.txt");
    fake_remove_file("ur0:tai/config.txt.original");
    fake_remove_file("ur0:tai/config.txt.session");
    editor_scan();
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    launch_card_build(&card);
    modal_open_safety(&ui, &card);
    if (!render(dir, "preview-safety-card-none.png", 2.62f, &ui))
        return 1;
    ui.modal = UI_MODAL_NONE;

    /* Unreadable config: locked, with the reason named. */
    reset_scene(&ui);
    fake_fail_read_path("ur0:tai/config.txt", 1);
    editor_scan();
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    launch_card_build(&card);
    modal_open_safety(&ui, &card);
    if (!render(dir, "preview-safety-card-unreadable.png", 2.63f, &ui))
        return 1;
    ui.modal = UI_MODAL_NONE;

    reset_scene(&ui);

    /* Low battery while discharging: the real alert path, not a forced flag. */
    fake_set_power(14, 0);
    metrics_sample_slow();
    if (!render(dir, "preview-alert.png", 4.0f, &ui))
        return 1;

    /* No CPU sample: every CORE value and both rates must render "--". */
    reset_scene(&ui);
    fake_set_cpu_absent();
    metrics_sample_slow();
    if (!render(dir, "preview-cpu-absent.png", 4.05f, &ui))
        return 1;

    /* Two active cores: the absent cores render "--", not 0 %. */
    reset_scene(&ui);
    fake_set_cpu_mask(0x5);
    metrics_sample_slow();
    if (!render(dir, "preview-cpu-partial.png", 4.1f, &ui))
        return 1;

    reset_scene(&ui);
    ui.vitals_scroll = (float)layout_max_scroll();
    if (!render(dir, "preview-vitals-plugins.png", 2.7f, &ui))
        return 1;

    reset_scene(&ui);
    fake_remove_file("ur0:tai/config.txt");
    editor_scan();
    plugins_scan();
    ui.vitals_scroll = (float)layout_max_scroll();
    if (!render(dir, "preview-vitals-plugins-empty.png", 2.8f, &ui))
        return 1;

    /* The editable ledger. */
    reset_scene(&ui);
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    if (!render(dir, "preview-plugins.png", 3.0f, &ui))
        return 1;

    /* The bottom of the ledger: the orphan section must be reachable. */
    reset_scene(&ui);
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = g_editor.row_count > 0 ? g_editor.row_count - 1 : -1;
    ui.plugins_scroll = (float)plugins_max_scroll();
    if (!render(dir, "preview-plugins-bottom.png", 3.05f, &ui))
        return 1;

    /* Header slots: normal, staged, restore preview, and every exception. */
    reset_scene(&ui);
    open_plugins(&ui);
    if (!render(dir, "preview-header-normal.png", 3.06f, &ui))
        return 1;

    /* A short list must not leave the pinned dock floating below the panel. */
    reset_scene(&ui);
    fake_remove_file("ur0:tai/config.txt");
    editor_scan();
    plugins_scan();
    open_plugins(&ui);
    if (!render(dir, "preview-plugins-empty.png", 3.065f, &ui))
        return 1;

    reset_scene(&ui);
    stage_batch(&ui);
    if (!render(dir, "preview-header-draft.png", 3.07f, &ui))
        return 1;

    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt.session", PREVIEW_SESSION_OFF_KUBRIDGE);
    (void)editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err));
    open_plugins(&ui);
    if (!render(dir, "preview-header-preview.png", 3.08f, &ui))
        return 1;

    reset_scene(&ui);
    {
        char big[EDITOR_LINES_MAX * 24 + 128];
        size_t off = 0;

        off += (size_t)snprintf(big + off, sizeof(big) - off,
                                "ur0:tai/stray.skprx\n*KERNEL\n");
        for (int i = 0; i < EDITOR_LINES_MAX + 1; ++i)
            off += (size_t)snprintf(big + off, sizeof(big) - off,
                                    "ur0:tai/p%03d.skprx\n", i);

        fake_add_file("ux0:tai/config.txt", big);
        fake_remove_file("ur0:tai/config.txt");
        fake_add_file("ur0:tai/config.txt.old",
                      "*main\nur0:tai/kubridge.skprx\n");
        fake_fail_backup(1);
        editor_scan();
        plugins_scan();
        fake_fail_backup(0);
        open_plugins(&ui);
    }
    if (!render(dir, "preview-header-exception.png", 3.09f, &ui))
        return 1;

    /* Row states: ON, OFF, MIXED, BOOT, MISSING, selection and a draft notch. */
    reset_scene(&ui);
    add_row_states_config();
    editor_scan();
    plugins_scan();
    open_plugins(&ui);
    ui.selection = editor_row_for_path("ur0:tai/kubridge.skprx");
    ui.plugins_scroll = 25.0f;
    if (!render(dir, "preview-rows.png", 3.10f, &ui))
        return 1;

    reset_scene(&ui);
    add_row_states_config();
    editor_scan();
    plugins_scan();
    open_plugins(&ui);
    ui.selection = editor_row_for_path("ur0:tai/mystery.suprx");
    (void)editor_stage_toggle(editor_row_for_path("ur0:tai/mystery.suprx"), 1,
                              err, (int)sizeof(err));
    (void)editor_stage_toggle(editor_row_for_path("ur0:tai/kubridge.skprx"), 0,
                              err, (int)sizeof(err));
    ui.plugins_scroll = 25.0f;
    if (!render(dir, "preview-rows-draft.png", 3.11f, &ui))
        return 1;

    /* LOCKED and NOT REACHED, with the selection and the draft notch. */
    reset_scene(&ui);
    add_row_locked_config();
    editor_scan();
    plugins_scan();
    open_plugins(&ui);
    ui.selection = editor_row_for_path("ur0:tai/mystery.suprx");
    (void)editor_stage_toggle(ui.selection, 0, err, (int)sizeof(err));
    if (!render(dir, "preview-rows-locked.png", 3.12f, &ui))
        return 1;

    /* Read-only restore list: proposed switches, dim unchanged rows, removals. */
    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt", PREVIEW_BASE_CONFIG);
    editor_scan();
    plugins_scan();
    fake_add_file("ur0:tai/config.txt.session", PREVIEW_SNAPSHOT);
    (void)editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err));
    open_plugins(&ui);
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 0;
    if (!render(dir, "preview-preview-list.png", 3.13f, &ui))
        return 1;

    ui.plugins_scroll = (float)plugins_max_scroll();
    if (!render(dir, "preview-preview-removed.png", 3.14f, &ui))
        return 1;

    reset_scene(&ui);
    open_plugins(&ui);
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 0;
    if (!render(dir, "preview-dock-idle.png", 3.15f, &ui))
        return 1;

    /* An idle restore failure has no state change; the strip must show it. */
    snprintf(ui.dock_message, sizeof(ui.dock_message), "NO RESTORE POINT");
    if (!render(dir, "preview-dock-idle-error.png", 3.155f, &ui))
        return 1;

    reset_scene(&ui);
    stage_batch(&ui);
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 0;
    if (!render(dir, "preview-dock-draft.png", 3.16f, &ui))
        return 1;

    /* Five changes exercise the comma glyph and the +N MORE overflow. */
    reset_scene(&ui);
    open_plugins(&ui);
    (void)editor_stage_toggle(editor_row_for_path("ur0:tai/kubridge.skprx"), 0,
                              err, (int)sizeof(err));
    (void)editor_stage_toggle(editor_row_for_path("ur0:tai/storagemgr.skprx"), 0,
                              err, (int)sizeof(err));
    (void)editor_stage_toggle(editor_row_for_path("ur0:tai/0syscall6.skprx"), 0,
                              err, (int)sizeof(err));
    (void)editor_stage_toggle(editor_row_for_path("ur0:tai/shellbat.suprx"), 1,
                              err, (int)sizeof(err));
    (void)editor_stage_toggle(editor_row_for_path("ur0:tai/mystery.suprx"), 1,
                              err, (int)sizeof(err));
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 2;
    if (!render(dir, "preview-dock-draft-many.png", 3.165f, &ui))
        return 1;

    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt", PREVIEW_BASE_CONFIG);
    editor_scan();
    plugins_scan();
    fake_add_file("ur0:tai/config.txt.session", PREVIEW_SNAPSHOT);
    (void)editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err));
    open_plugins(&ui);
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 0;
    if (!render(dir, "preview-dock-preview.png", 3.17f, &ui))
        return 1;

    /* A preview opened over a draft names what the confirm discards (R42). */
    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt", PREVIEW_BASE_CONFIG);
    editor_scan();
    plugins_scan();
    (void)editor_stage_toggle(editor_row_for_path("ur0:tai/shellbat.suprx"), 1,
                              err, (int)sizeof(err));
    fake_add_file("ur0:tai/config.txt.session", PREVIEW_SNAPSHOT);
    (void)editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err));
    open_plugins(&ui);
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 1;
    if (!render(dir, "preview-dock-preview-draft.png", 3.175f, &ui))
        return 1;

    reset_scene(&ui);
    open_plugins(&ui);
    (void)editor_stage_toggle(editor_row_for_path("ur0:tai/kubridge.skprx"), 0,
                              err, (int)sizeof(err));
    (void)editor_commit_draft(err, (int)sizeof(err));
    open_plugins(&ui);
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 1;
    if (!render(dir, "preview-dock-saved.png", 3.18f, &ui))
        return 1;

    reset_scene(&ui);
    open_plugins(&ui);
    (void)editor_stage_toggle(editor_row_for_path("ur0:tai/kubridge.skprx"), 0,
                              err, (int)sizeof(err));
    (void)editor_commit_draft(err, (int)sizeof(err));
    editor_scan();
    plugins_scan();
    open_plugins(&ui);
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 0;
    if (!render(dir, "preview-dock-last-write.png", 3.19f, &ui))
        return 1;

    reset_scene(&ui);
    stage_batch(&ui);
    fake_set_power(14, 0);
    metrics_sample_slow();
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 1;
    if (!render(dir, "preview-dock-battery-blocked.png", 3.20f, &ui))
        return 1;

    reset_scene(&ui);
    stage_batch(&ui);
    fake_set_power_absent();
    metrics_sample_slow();
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 1;
    if (!render(dir, "preview-dock-battery-unknown.png", 3.21f, &ui))
        return 1;

    reset_scene(&ui);
    (void)editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err));
    open_plugins(&ui);
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 0;
    if (!render(dir, "preview-dock-no-changes.png", 3.22f, &ui))
        return 1;

    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt.session", PREVIEW_SESSION_NO_HENKAKU);
    (void)editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err));
    open_plugins(&ui);
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 0;
    ui.plugins_scroll = (float)plugins_max_scroll();
    if (!render(dir, "preview-dock-locked-removal.png", 3.23f, &ui))
        return 1;

    reset_scene(&ui);
    stage_batch(&ui);
    snprintf(ui.dock_message, sizeof(ui.dock_message), "WRITE FAILED DATA");
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 1;
    if (!render(dir, "preview-dock-write-error.png", 3.24f, &ui))
        return 1;

    /* A stray path is a warning: the valid rows stay editable. */
    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt",
                  "ur0:tai/stray.skprx\n"
                  "*KERNEL\nur0:tai/kubridge.skprx\n"
                  "*main\n#ur0:tai/shellbat.suprx\n");
    editor_scan();
    plugins_scan();
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    if (!render(dir, "preview-warning.png", 3.08f, &ui))
        return 1;

    /* Missing active and missing inactive rows: one can be disabled, one
     * cannot be enabled, and each shows the control state. */
    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt",
                  "*KERNEL\nur0:tai/absent.skprx\n"
                  "#ur0:tai/also-absent.skprx\n"
                  "*main\nur0:tai/kubridge.skprx\n"
                  "#ur0:tai/shellbat.suprx\n");
    editor_scan();
    plugins_scan();
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    if (!render(dir, "preview-missing.png", 3.1f, &ui))
        return 1;

    /* A failed snapshot is a warning, not a lock. */
    reset_scene(&ui);
    fake_fail_backup(1);
    editor_scan();
    plugins_scan();
    fake_fail_backup(0);
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    if (!render(dir, "preview-backup-warning.png", 3.2f, &ui))
        return 1;

    /* Five groups: the summary block must keep every group on one line. */
    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt",
                  "# five groups\n"
                  "*KERNEL\nur0:tai/kubridge.skprx\n"
                  "*main\nur0:tai/henkaku.suprx\n"
                  "*NPXS10015\nur0:tai/yamt.suprx\n"
                  "*ALL\nur0:tai/vita3g-disable.suprx\n"
                  "*PCSE00001\nur0:tai/extra.skprx\n");
    editor_scan();
    plugins_scan();
    ui.tab = UI_TAB_VITALS;
    ui.vitals_scroll = (float)layout_max_scroll();
    if (!render(dir, "preview-groups.png", 3.3f, &ui))
        return 1;

    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt",
                  "*main\nur0:tai/kubridge.skprx\nux0:tai/kubridge.skprx\n");
    fake_add_file("ux0:tai/kubridge.skprx", "");
    editor_scan();
    plugins_scan();
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    if (!render(dir, "preview-duplicate.png", 3.5f, &ui))
        return 1;

    reset_scene(&ui);
    ui.tab = UI_TAB_PLUGINS;
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    ui.selection = row;
    stage_commit(row, 0);
    if (!render(dir, "preview-plugins-next.png", 3.7f, &ui))
        return 1;

    /* The SAVE chip's inline R35 warning (the chip is focused, not pressed). */
    reset_scene(&ui);
    open_plugins(&ui);
    (void)editor_stage_toggle(editor_row_for_path("ur0:tai/shellbat.suprx"), 1,
                              err, (int)sizeof(err));
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 1;
    if (!render(dir, "preview-dock-save-only.png", 3.75f, &ui))
        return 1;

    reset_scene(&ui);
    make_long_config();
    editor_scan();
    plugins_scan();
    ui.tab = UI_TAB_PLUGINS;
    ui.selection = 0;
    if (!render(dir, "preview-readonly.png", 5.5f, &ui))
        return 1;

    reset_scene(&ui);
    fake_add_file("ur0:tai/config.txt",
                  "ur0:tai/stray.skprx\n"
                  "*KERNEL\nur0:tai/kubridge.skprx\n"
                  "*main\n#ur0:tai/shellbat.suprx\n");
    editor_scan();
    plugins_scan();
    ui.tab = UI_TAB_VITALS;
    ui.vitals_scroll = (float)layout_max_scroll();
    if (!render(dir, "preview-vitals-plugins-warning.png", 6.0f, &ui))
        return 1;

    /* ABOUT: identity, the guarded reboot, and the real power-off path. */
    reset_scene(&ui);
    ui.tab = UI_TAB_ABOUT;
    if (!render(dir, "preview-about.png", 7.0f, &ui))
        return 1;

    reset_scene(&ui);
    ui.tab = UI_TAB_ABOUT;
    fake_set_power(15, 0);
    metrics_sample_slow();
    if (!render(dir, "preview-about-blocked.png", 7.1f, &ui))
        return 1;

    reset_scene(&ui);
    ui.tab = UI_TAB_ABOUT;
    fake_set_power_absent();
    metrics_sample_slow();
    if (!render(dir, "preview-about-unknown.png", 7.2f, &ui))
        return 1;

    /* The ABOUT reboot confirmation, one scene per pending state. */
    reset_scene(&ui);
    ui.tab = UI_TAB_ABOUT;
    int reboot_row = editor_row_for_path("ur0:tai/kubridge.skprx");
    (void)editor_stage_toggle(reboot_row, 0, err, (int)sizeof(err));
    about_reboot_text(title, sizeof(title), body, sizeof(body));
    modal_open_reboot(&ui, title, body);
    if (!render(dir, "preview-reboot-draft.png", 7.3f, &ui))
        return 1;

    reset_scene(&ui);
    ui.tab = UI_TAB_ABOUT;
    reboot_row = editor_row_for_path("ur0:tai/kubridge.skprx");
    (void)editor_stage_toggle(reboot_row, 0, err, (int)sizeof(err));
    (void)editor_preview_restore(EDITOR_SNAPSHOT_ORIGINAL, err,
                                 (int)sizeof(err));
    about_reboot_text(title, sizeof(title), body, sizeof(body));
    modal_open_reboot(&ui, title, body);
    if (!render(dir, "preview-reboot-preview-draft.png", 7.31f, &ui))
        return 1;

    reset_scene(&ui);
    ui.tab = UI_TAB_ABOUT;
    (void)editor_preview_restore(EDITOR_SNAPSHOT_ORIGINAL, err,
                                 (int)sizeof(err));
    about_reboot_text(title, sizeof(title), body, sizeof(body));
    modal_open_reboot(&ui, title, body);
    if (!render(dir, "preview-reboot-preview-clean.png", 7.32f, &ui))
        return 1;

    reset_scene(&ui);
    ui.tab = UI_TAB_ABOUT;
    reboot_row = editor_row_for_path("ur0:tai/kubridge.skprx");
    stage_commit(reboot_row, 0);
    about_reboot_text(title, sizeof(title), body, sizeof(body));
    modal_open_reboot(&ui, title, body);
    if (!render(dir, "preview-reboot-saved.png", 7.33f, &ui))
        return 1;

    return 0;
}
