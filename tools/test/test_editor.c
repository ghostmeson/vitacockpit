#include <stdio.h>
#include <string.h>

#include "core/input.h"
#include "core/metrics.h"
#include "core/plugin_editor.h"
#include "core/platform.h"
#include "core/tai_config.h"
#include "fake_platform.h"
#include "ui/plugin_flow.h"
#include "ui/plugin_nav.h"

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                      \
    do {                                                                 \
        ++g_checks;                                                      \
        if (!(cond)) {                                                   \
            ++g_failures;                                                \
            fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__,      \
                    #cond);                                              \
        }                                                                \
    } while (0)

#define SAMPLE                                                              \
    "# sample tai config\n"                                                 \
    "*KERNEL\n"                                                             \
    "ur0:tai/kubridge.skprx\n"                                              \
    "ur0:tai/storagemgr.skprx\n"                                           \
    "ur0:tai/0syscall6.skprx\n"                                            \
    "*main\n"                                                               \
    "ur0:tai/henkaku.suprx\n"                                              \
    "ur0:tai/libshacccg.suprx\n"                                           \
    "#ur0:tai/shellbat.suprx\n"                                            \
    "#ur0:tai/mystery.suprx\n"                                             \
    "*NPXS10015\n"                                                          \
    "ur0:tai/yamt.suprx\n"                                                 \
    "*ALL\n"                                                                \
    "ur0:tai/vita3g-disable.suprx\n"

#define SAMPLE_BOOT "- load\tur0:tai/taihen.skprx\n- load\tur0:tai/henkaku.skprx\n"

#define SAMPLE_OFF_KUBRIDGE                                                 \
    "# sample tai config\n"                                                 \
    "*KERNEL\n"                                                             \
    "#ur0:tai/kubridge.skprx\n"                                             \
    "ur0:tai/storagemgr.skprx\n"                                            \
    "ur0:tai/0syscall6.skprx\n"                                             \
    "*main\n"                                                               \
    "ur0:tai/henkaku.suprx\n"                                               \
    "ur0:tai/libshacccg.suprx\n"                                            \
    "#ur0:tai/shellbat.suprx\n"                                             \
    "#ur0:tai/mystery.suprx\n"                                              \
    "*NPXS10015\n"                                                          \
    "ur0:tai/yamt.suprx\n"                                                  \
    "*ALL\n"                                                                \
    "ur0:tai/vita3g-disable.suprx\n"

#define SAMPLE_SHELLBAT_ON                                                  \
    "# sample tai config\n"                                                 \
    "*KERNEL\n"                                                             \
    "ur0:tai/kubridge.skprx\n"                                              \
    "ur0:tai/storagemgr.skprx\n"                                            \
    "ur0:tai/0syscall6.skprx\n"                                             \
    "*main\n"                                                               \
    "ur0:tai/henkaku.suprx\n"                                               \
    "ur0:tai/libshacccg.suprx\n"                                            \
    "ur0:tai/shellbat.suprx\n"                                              \
    "#ur0:tai/mystery.suprx\n"                                              \
    "*NPXS10015\n"                                                          \
    "ur0:tai/yamt.suprx\n"                                                  \
    "*ALL\n"                                                                \
    "ur0:tai/vita3g-disable.suprx\n"

#define SAMPLE_NO_HENKAKU                                                   \
    "# sample tai config\n"                                                 \
    "*KERNEL\n"                                                             \
    "ur0:tai/kubridge.skprx\n"                                              \
    "ur0:tai/storagemgr.skprx\n"                                            \
    "ur0:tai/0syscall6.skprx\n"                                             \
    "*main\n"                                                               \
    "ur0:tai/libshacccg.suprx\n"                                            \
    "#ur0:tai/shellbat.suprx\n"                                             \
    "#ur0:tai/mystery.suprx\n"                                              \
    "*NPXS10015\n"                                                          \
    "ur0:tai/yamt.suprx\n"                                                  \
    "*ALL\n"                                                                \
    "ur0:tai/vita3g-disable.suprx\n"

#define SAMPLE_NO_SHELLBAT                                                  \
    "# sample tai config\n"                                                 \
    "*KERNEL\n"                                                             \
    "ur0:tai/kubridge.skprx\n"                                              \
    "ur0:tai/storagemgr.skprx\n"                                            \
    "ur0:tai/0syscall6.skprx\n"                                             \
    "*main\n"                                                               \
    "ur0:tai/henkaku.suprx\n"                                               \
    "ur0:tai/libshacccg.suprx\n"                                            \
    "#ur0:tai/mystery.suprx\n"                                              \
    "*NPXS10015\n"                                                          \
    "ur0:tai/yamt.suprx\n"                                                  \
    "*ALL\n"                                                                \
    "ur0:tai/vita3g-disable.suprx\n"

#define SAMPLE_HENKAKU_OFF                                                  \
    "# sample tai config\n"                                                 \
    "*KERNEL\n"                                                             \
    "ur0:tai/kubridge.skprx\n"                                              \
    "ur0:tai/storagemgr.skprx\n"                                            \
    "ur0:tai/0syscall6.skprx\n"                                             \
    "*main\n"                                                               \
    "#ur0:tai/henkaku.suprx\n"                                              \
    "ur0:tai/libshacccg.suprx\n"                                            \
    "#ur0:tai/shellbat.suprx\n"                                             \
    "#ur0:tai/mystery.suprx\n"                                              \
    "*NPXS10015\n"                                                          \
    "ur0:tai/yamt.suprx\n"                                                  \
    "*ALL\n"                                                                \
    "ur0:tai/vita3g-disable.suprx\n"

/* A `*!` halt before HENKAKU: present in the file, but never reached. */
#define SAMPLE_HENKAKU_HALTED                                               \
    "# sample tai config\n"                                                 \
    "*KERNEL\n"                                                             \
    "ur0:tai/kubridge.skprx\n"                                              \
    "ur0:tai/storagemgr.skprx\n"                                            \
    "ur0:tai/0syscall6.skprx\n"                                             \
    "*main\n"                                                               \
    "*!ALL\n"                                                               \
    "ur0:tai/henkaku.suprx\n"                                               \
    "ur0:tai/libshacccg.suprx\n"                                            \
    "#ur0:tai/shellbat.suprx\n"                                             \
    "#ur0:tai/mystery.suprx\n"                                              \
    "*NPXS10015\n"                                                          \
    "ur0:tai/yamt.suprx\n"                                                  \
    "*ALL\n"                                                                \
    "ur0:tai/vita3g-disable.suprx\n"

/* Two enables and one disable committed as one batch. */
#define SAMPLE_BATCH                                                        \
    "# sample tai config\n"                                                 \
    "*KERNEL\n"                                                             \
    "#ur0:tai/kubridge.skprx\n"                                             \
    "ur0:tai/storagemgr.skprx\n"                                            \
    "ur0:tai/0syscall6.skprx\n"                                             \
    "*main\n"                                                               \
    "ur0:tai/henkaku.suprx\n"                                               \
    "ur0:tai/libshacccg.suprx\n"                                            \
    "ur0:tai/shellbat.suprx\n"                                              \
    "ur0:tai/mystery.suprx\n"                                               \
    "*NPXS10015\n"                                                          \
    "ur0:tai/yamt.suprx\n"                                                  \
    "*ALL\n"                                                                \
    "ur0:tai/vita3g-disable.suprx\n"

/* The launch file with one loaded, non-protected path removed. */
#define SAMPLE_NO_LIBSHACCCG                                                \
    "# sample tai config\n"                                                 \
    "*KERNEL\n"                                                             \
    "ur0:tai/kubridge.skprx\n"                                              \
    "ur0:tai/storagemgr.skprx\n"                                            \
    "ur0:tai/0syscall6.skprx\n"                                             \
    "*main\n"                                                               \
    "ur0:tai/henkaku.suprx\n"                                               \
    "#ur0:tai/shellbat.suprx\n"                                             \
    "#ur0:tai/mystery.suprx\n"                                              \
    "*NPXS10015\n"                                                          \
    "ur0:tai/yamt.suprx\n"                                                  \
    "*ALL\n"                                                                \
    "ur0:tai/vita3g-disable.suprx\n"

#define SAMPLE_PLUS_EXTRA SAMPLE "ur0:tai/extra.skprx\n"

#define LAYOUT_SNAPSHOT                                                     \
    "*KERNEL\n"                                                             \
    "ur0:tai/storagemgr.skprx\n"                                            \
    "#ur0:tai/mystery.suprx\n"                                              \
    "ur0:tai/0syscall6.skprx\n"                                             \
    "*main\n"                                                               \
    "ur0:tai/henkaku.suprx\n"                                               \
    "ur0:tai/shellbat.suprx\n"                                              \
    "ur0:tai/libshacccg.suprx\n"                                            \
    "ur0:tai/kubridge.skprx\n"                                              \
    "*NPXS10015\n"                                                          \
    "ur0:tai/yamt.suprx\n"                                                  \
    "*ALL\n"                                                                \
    "ur0:tai/vita3g-disable.suprx\n"

static int text_is(const char *path, const char *expected)
{
    static char buf[32768];
    int n = 0;

    if (platform_read_config(path, buf, (int)sizeof(buf), &n) !=
        PLATFORM_READ_OK)
        return 0;

    return strcmp(buf, expected) == 0;
}

static int read_file(const char *path, char *buf, int cap)
{
    int n = 0;

    if (platform_read_config(path, buf, cap, &n) != PLATFORM_READ_OK)
        return 0;

    return n;
}

/*
 * The old per-row write path is gone; the app is a stage followed by a dock
 * commit. These two helpers keep the editor tests exercising the same pair
 * without a UI.
 */
static EditorWriteResult stage_commit(int row, int mode, char *err, int err_cap)
{
    EditorWriteResult result = editor_stage_toggle(row, mode, err, err_cap);

    if (result != EDITOR_WRITE_OK)
        return result;

    return editor_commit_draft(err, err_cap);
}

static EditorWriteResult preview_commit(EditorSnapshot kind, char *err,
                                        int err_cap)
{
    EditorWriteResult result = editor_preview_restore(kind, err, err_cap);

    if (result != EDITOR_WRITE_OK)
        return result;

    result = editor_commit_preview(err, err_cap);

    /* A refused commit leaves the preview open for the next retry; these
     * per-call tests restore the pre-preview working set before returning. */
    if (result != EDITOR_WRITE_OK && g_editor.preview_active)
        editor_cancel_preview();

    return result;
}

/* The cached launch activity for a path, or -1 when the path is unknown. */
static int file_launch_active(const char *path)
{
    return editor_launch_active_for_path(path);
}

static int file_row_known(const char *path)
{
    return editor_launch_active_for_path(path) >= 0;
}

static void test_scan_sample(void)
{
    fake_reset();
    editor_scan();

    CHECK(g_editor.source_ok == 1);
    CHECK(strcmp(g_editor.source, "ur0:tai/config.txt") == 0);
    CHECK(g_editor.valid == 1);
    CHECK(g_editor.row_count == 9);
    CHECK(g_editor.orphan_count == 1);
    CHECK(strcmp(g_editor.orphans[0], "ur0:tai/extra.skprx") == 0);
    CHECK(g_editor.loaded_total == 7);

    CHECK(text_is("ur0:tai/config.txt.original", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt.session", SAMPLE));
    CHECK(text_is("ur0:tai/boot_config.txt", SAMPLE_BOOT));
    CHECK(!platform_file_exists("ur0:tai/boot_config.txt.bak"));
    CHECK(!platform_file_exists("ur0:tai/boot_config.txt.original"));
    CHECK(!platform_file_exists("ur0:tai/boot_config.txt.session"));

    int henkaku = editor_row_for_path("ur0:tai/henkaku.suprx");
    CHECK(henkaku >= 0);
    CHECK(g_editor.rows[henkaku].locked == 1);
    CHECK(editor_next_action(henkaku) == -1);
    CHECK(g_editor.rows[henkaku].status == EDITOR_LOADED);

    int storage = editor_row_for_path("ur0:tai/storagemgr.skprx");
    CHECK(storage >= 0);
    CHECK(editor_confirm_text(storage, 0) != NULL);
    CHECK(editor_confirm_text(storage, 1) != NULL);

    CHECK(editor_confirm_text(editor_row_for_path("ur0:tai/kubridge.skprx"),
                              1) == NULL);

    int shellbat = editor_row_for_path("ur0:tai/shellbat.suprx");
    CHECK(shellbat >= 0);
    CHECK(g_editor.rows[shellbat].status == EDITOR_DISABLED);
    CHECK(editor_next_action(shellbat) == 1);
    CHECK(g_editor.rows[shellbat].active_lines == 0);
    CHECK(g_editor.rows[shellbat].reached_lines == 1);

    int yamt = editor_row_for_path("ur0:tai/yamt.suprx");
    CHECK(yamt >= 0);
    CHECK(g_editor.rows[yamt].status == EDITOR_LOADED);
    CHECK(editor_next_action(yamt) == 0);
}

static void test_toggle_round_trip(void)
{
    fake_reset();
    editor_scan();

    char before[32768];
    CHECK(read_file("ur0:tai/config.txt", before, (int)sizeof(before)) > 0);

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(row == 0);
    CHECK(editor_next_action(row) == 0);
    CHECK(editor_confirm_text(row, 0) == NULL);

    char err[96];
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_LOADED);
    CHECK(g_editor.pending_count == 1);

    CHECK(text_is("ur0:tai/config.txt.original", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt.session", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt.bak", before) == 0);

    CHECK(text_is("ur0:tai/config.txt",
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
                  "ur0:tai/vita3g-disable.suprx\n"));

    CHECK(stage_commit(row, 1, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_LOADED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_LOADED);
    CHECK(g_editor.pending_count == 0);
    CHECK(text_is("ur0:tai/config.txt", before));
}

/*
 * A switch press is a draft. The card and its presence cache are untouched
 * until a commit, and a stage and its reverse leave no draft at all.
 */
static void test_stage_no_write(void)
{
    char err[96];

    fake_reset();
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    int writes = fake_write_count();
    int stats = fake_stat_count();
    int active = g_editor.active_total;
    int disabled = g_editor.disabled_total;

    CHECK(active == 7);
    CHECK(disabled == 2);

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(g_editor.draft_dirty == 1);
    CHECK(g_editor.draft_count == 1);
    CHECK(editor_draft_dirty() == 1);
    CHECK(editor_draft_count() == 1);
    CHECK(editor_draft_change(row) == -1);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_LOADED);
    CHECK(g_editor.rows[row].loaded == 1);
    CHECK(g_editor.rows[row].next_change == -1);
    CHECK(g_editor.pending_count == 1);
    CHECK(file_launch_active("ur0:tai/kubridge.skprx") == 1);
    CHECK(g_editor.active_total == active - 1);
    CHECK(g_editor.disabled_total == disabled + 1);
    CHECK(g_editor.text_len == g_editor.last_len + 1);
    CHECK(fake_write_count() == writes);
    CHECK(fake_stat_count() == stats);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));

    /* The reverse returns the working set to the file exactly. */
    CHECK(editor_stage_toggle(row, 1, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(g_editor.draft_dirty == 0);
    CHECK(g_editor.draft_count == 0);
    CHECK(editor_draft_change(row) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_LOADED);
    CHECK(g_editor.rows[row].next_change == 0);
    CHECK(g_editor.pending_count == 0);
    CHECK(g_editor.active_total == active);
    CHECK(g_editor.disabled_total == disabled);
    CHECK(fake_write_count() == writes);
    CHECK(fake_stat_count() == stats);

    /* Discard restores the file state without writing or statting. */
    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    editor_discard_draft();
    CHECK(g_editor.draft_dirty == 0);
    CHECK(g_editor.draft_count == 0);
    CHECK(g_editor.rows[row].status == EDITOR_LOADED);
    CHECK(g_editor.pending_count == 0);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(fake_write_count() == writes);
    CHECK(fake_stat_count() == stats);
    CHECK(!platform_file_exists("ur0:tai/config.txt.undo"));
}

/* Two enables and one disable are one batch, still in memory. */
static void test_stage_batch(void)
{
    char err[96];

    fake_reset();
    editor_scan();

    int kernel = editor_row_for_path("ur0:tai/kubridge.skprx");
    int shell = editor_row_for_path("ur0:tai/shellbat.suprx");
    int mystery = editor_row_for_path("ur0:tai/mystery.suprx");
    int writes = fake_write_count();

    CHECK(editor_stage_toggle(kernel, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_stage_toggle(shell, 1, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_stage_toggle(mystery, 1, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);

    CHECK(editor_draft_count() == 3);
    CHECK(g_editor.pending_count == 3);
    CHECK(editor_draft_change(kernel) == -1);
    CHECK(editor_draft_change(shell) == 1);
    CHECK(editor_draft_change(mystery) == 1);
    CHECK(g_editor.active_total == 8);
    CHECK(g_editor.disabled_total == 1);
    CHECK(g_editor.rows[kernel].next_change == -1);
    CHECK(g_editor.rows[shell].next_change == 1);
    CHECK(g_editor.rows[mystery].next_change == 1);
    CHECK(fake_write_count() == writes);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));

    /* The whole batch is one undo and one live write, never one per row. */
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(fake_write_count() == writes + 2);
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt", SAMPLE_BATCH));
    CHECK(g_editor.draft_dirty == 0);
    CHECK(editor_draft_count() == 0);
    CHECK(g_editor.pending_count == 3);
    CHECK(g_editor.rows[kernel].next_change == -1);
    CHECK(g_editor.rows[shell].next_change == 1);
    CHECK(g_editor.rows[mystery].next_change == 1);
}

/* One commit: one undo snapshot, one live write, then the draft clears. */
static void test_commit_draft(void)
{
    char err[96];

    fake_reset();
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);

    int writes = fake_write_count();

    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(err[0] == '\0');
    CHECK(fake_write_count() == writes + 2);
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt", SAMPLE_OFF_KUBRIDGE));
    CHECK(g_editor.draft_dirty == 0);
    CHECK(g_editor.draft_count == 0);
    CHECK(editor_draft_change(row) == 0);
    CHECK(g_editor.pending_count == 1);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_LOADED);
    CHECK(g_editor.rows[row].loaded == 1);
    CHECK(g_editor.rows[row].next_change == -1);
    CHECK(g_editor.undo_available == 1);
    CHECK(g_editor.active_total == 6);
    CHECK(g_editor.disabled_total == 3);
    CHECK(strcmp(g_editor.last_text, SAMPLE_OFF_KUBRIDGE) == 0);

    /* A second commit with an empty draft writes nothing. */
    writes = fake_write_count();
    CHECK(editor_commit_draft(err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "NO CHANGES") != NULL);
    CHECK(fake_write_count() == writes);
}

/* An external edit refuses the commit and leaves the draft intact (R5). */
static void test_commit_stale_refused(void)
{
    char err[96];

    fake_reset();
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);

    int writes = fake_write_count();

    fake_add_file("ur0:tai/config.txt", "# changed elsewhere\n");
    CHECK(editor_commit_draft(err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "CONFIG CHANGED") != NULL);
    CHECK(fake_write_count() == writes);
    CHECK(text_is("ur0:tai/config.txt", "# changed elsewhere\n"));
    CHECK(g_editor.draft_dirty == 1);
    CHECK(g_editor.draft_count == 1);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_LOADED);
}

/* Commit and discard both return early when there is nothing staged. */
static void test_empty_draft(void)
{
    char err[96];

    fake_reset();
    editor_scan();

    int writes = fake_write_count();
    int stats = fake_stat_count();

    CHECK(editor_commit_draft(err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "NO CHANGES") != NULL);
    CHECK(fake_write_count() == writes);

    editor_discard_draft();

    CHECK(g_editor.draft_dirty == 0);
    CHECK(g_editor.draft_count == 0);
    CHECK(fake_write_count() == writes);
    CHECK(fake_stat_count() == stats);
}

/*
 * A row that was MIXED at launch keeps that launch badge when the working set
 * is switched fully off; the rocker and title follow the working set.
 */
static void test_launch_partial(void)
{
    char err[96];

    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\nur0:tai/kubridge.skprx\n"
                  "*NPXS10015\n#ur0:tai/kubridge.skprx\n");
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(g_editor.rows[row].status == EDITOR_PARTIAL);
    CHECK(g_editor.rows[row].launch_status == EDITOR_PARTIAL);
    CHECK(g_editor.rows[row].active_lines == 1);
    CHECK(g_editor.rows[row].reached_lines == 2);

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_PARTIAL);
    CHECK(g_editor.rows[row].active_lines == 0);
    CHECK(g_editor.rows[row].next_change == -1);
    CHECK(editor_draft_change(row) == -1);

    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_PARTIAL);
    CHECK(g_editor.pending_count == 1);

    CHECK(editor_stage_toggle(row, 1, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(g_editor.rows[row].status == EDITOR_LOADED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_PARTIAL);
    CHECK(g_editor.rows[row].active_lines == 2);
    CHECK(g_editor.rows[row].next_change == 1);
    CHECK(editor_draft_change(row) == 1);
}

/*
 * The undo snapshot has a different line layout. The launch facts must follow
 * the paths; a line-index copy would put LOADED on the wrong rows.
 */
static void test_restore_layout_keeps_launch(void)
{
    static const char snapshot[] =
        "*KERNEL\n"
        "ur0:tai/storagemgr.skprx\n"
        "#ur0:tai/mystery.suprx\n"
        "ur0:tai/0syscall6.skprx\n"
        "*main\n"
        "ur0:tai/henkaku.suprx\n"
        "ur0:tai/shellbat.suprx\n"
        "ur0:tai/libshacccg.suprx\n"
        "ur0:tai/kubridge.skprx\n"
        "*NPXS10015\n"
        "ur0:tai/yamt.suprx\n"
        "*ALL\n"
        "ur0:tai/vita3g-disable.suprx\n";

    char err[96];

    fake_reset();
    editor_scan();

    CHECK(platform_snapshot_undo("ur0:tai/config.txt", snapshot,
                                 (int)strlen(snapshot)) == 0);
    CHECK(preview_commit(EDITOR_SNAPSHOT_UNDO, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt", snapshot));

    int kubridge = editor_row_for_path("ur0:tai/kubridge.skprx");
    int shellbat = editor_row_for_path("ur0:tai/shellbat.suprx");
    int mystery = editor_row_for_path("ur0:tai/mystery.suprx");

    CHECK(g_editor.rows[kubridge].launch_status == EDITOR_LOADED);
    CHECK(g_editor.rows[kubridge].status == EDITOR_LOADED);
    CHECK(g_editor.rows[kubridge].next_change == 0);
    CHECK(g_editor.rows[shellbat].launch_status == EDITOR_DISABLED);
    CHECK(g_editor.rows[shellbat].status == EDITOR_LOADED);
    CHECK(g_editor.rows[shellbat].next_change == 1);
    CHECK(g_editor.rows[mystery].launch_status == EDITOR_DISABLED);
    CHECK(g_editor.rows[mystery].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[mystery].next_change == 0);
    CHECK(g_editor.draft_dirty == 0);
    CHECK(g_editor.undo_available == 0);
}

/* Read-only, locked, halted, and missing-off rows stage nothing. */
static void test_stage_refusals(void)
{
    char cfg[EDITOR_LINES_MAX * 24 + 64];
    char err[96];

    /* The wrong mode is refused even when the row has an action. */
    fake_reset();
    editor_scan();
    int active = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(active >= 0);
    CHECK(editor_next_action(active) == EDITOR_ACTION_DISABLE);
    CHECK(editor_stage_toggle(active, EDITOR_ACTION_ENABLE, err,
                              (int)sizeof(err)) == EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "NO ACTION") != NULL);
    CHECK(g_editor.draft_dirty == 0);

    /* A negative capacity must not be cast to a huge size_t. */
    char tiny[1] = {'x'};
    CHECK(editor_stage_toggle(-1, EDITOR_ACTION_DISABLE, tiny, -1) ==
          EDITOR_WRITE_REFUSED);
    CHECK(tiny[0] == 'x');

    size_t off = 0;
    off += (size_t)snprintf(cfg + off, sizeof(cfg) - off, "*KERNEL\n");
    for (int i = 0; i < EDITOR_LINES_MAX + 1; ++i) {
        off += (size_t)snprintf(cfg + off, sizeof(cfg) - off,
                                "ur0:tai/p%03d.skprx\n", i);
    }

    fake_reset();
    fake_add_file("ur0:tai/config.txt", cfg);
    editor_scan();
    CHECK(g_editor.read_only == 1);
    CHECK(editor_stage_toggle(0, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(g_editor.draft_dirty == 0);

    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*main\n#ur0:tai/henkaku.suprx\n");
    editor_scan();
    int locked = editor_row_for_path("ur0:tai/henkaku.suprx");
    CHECK(editor_stage_toggle(locked, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "HENKAKU") != NULL);
    CHECK(g_editor.draft_dirty == 0);
    CHECK(editor_stage_toggle(locked, 1, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(g_editor.draft_dirty == 1);
    CHECK(g_editor.draft_count == 1);
    CHECK(editor_draft_change(locked) == 1);
    CHECK(g_editor.rows[locked].launch_status == EDITOR_DISABLED);
    editor_discard_draft();
    CHECK(g_editor.draft_dirty == 0);

    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*KERNEL\nur0:tai/kubridge.skprx\n*!ALL\n"
                  "ur0:tai/extra.skprx\n");
    editor_scan();
    int halt = editor_row_for_path("ur0:tai/extra.skprx");
    CHECK(editor_stage_toggle(halt, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(g_editor.draft_dirty == 0);

    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*KERNEL\n#ur0:tai/absent.skprx\n");
    editor_scan();
    int missing = editor_row_for_path("ur0:tai/absent.skprx");
    CHECK(g_editor.rows[missing].present == 0);
    CHECK(editor_next_action(missing) == EDITOR_ACTION_NONE);
    CHECK(editor_stage_toggle(missing, 1, err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "FILE IS MISSING") != NULL);
    CHECK(g_editor.draft_dirty == 0);
    CHECK(editor_stage_toggle(missing, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(g_editor.draft_dirty == 0);
}

/* One path in two sections is one row and one draft change. */
static void test_stage_two_sections(void)
{
    char err[96];

    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\nur0:tai/kubridge.skprx\n"
                  "*NPXS10015\nur0:tai/kubridge.skprx\n");
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(g_editor.row_count == 1);
    CHECK(g_editor.rows[row].active_lines == 2);

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_draft_count() == 1);
    CHECK(editor_draft_change(row) == -1);
    CHECK(text_is("ur0:tai/config.txt",
                  "*main\nur0:tai/kubridge.skprx\n"
                  "*NPXS10015\nur0:tai/kubridge.skprx\n"));

    int marks = 0;
    for (const char *p = g_editor.text;
         (p = strstr(p, "#ur0:tai/kubridge.skprx")) != NULL; ++p)
        ++marks;
    CHECK(marks == 2);

    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt",
                  "*main\n#ur0:tai/kubridge.skprx\n"
                  "*NPXS10015\n#ur0:tai/kubridge.skprx\n"));
}

static void test_hash_exactness(void)
{
    char err[96];

    /* Indentation stays; the '#' goes immediately before the path. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\n  ur0:tai/kubridge.skprx\t\n");
    editor_scan();
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(row >= 0);
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt", "*main\n  #ur0:tai/kubridge.skprx\t\n"));
    CHECK(stage_commit(row, 1, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt", "*main\n  ur0:tai/kubridge.skprx\t\n"));

    /* CRLF and a missing final newline survive. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\r\nur0:tai/kubridge.skprx\r\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt",
                  "*main\r\n#ur0:tai/kubridge.skprx\r\n"));

    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*main\nur0:tai/kubridge.skprx");
    editor_scan();
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt", "*main\n#ur0:tai/kubridge.skprx"));

    /* A double-commented line enables by removing the whole run. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*main\n##ur0:tai/kubridge.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(row >= 0);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(stage_commit(row, 1, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt", "*main\nur0:tai/kubridge.skprx\n"));

    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*main\n# # ur0:tai/kubridge.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(stage_commit(row, 1, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt", "*main\nur0:tai/kubridge.skprx\n"));

    /* One path in two sections is one row and both lines move together. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\nur0:tai/kubridge.skprx\n"
                  "*NPXS10015\nur0:tai/kubridge.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(g_editor.row_count == 1);
    CHECK(g_editor.rows[row].active_lines == 2);
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt",
                  "*main\n#ur0:tai/kubridge.skprx\n"
                  "*NPXS10015\n#ur0:tai/kubridge.skprx\n"));
}

static void test_backups(void)
{
    fake_reset();
    editor_scan();

    CHECK(text_is("ur0:tai/config.txt.original", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt.session", SAMPLE));

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    char err[96];
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);

    /* A second session refreshes the session snapshot, never the original. */
    editor_scan();
    CHECK(text_is("ur0:tai/config.txt.original", SAMPLE));
    CHECK(!text_is("ur0:tai/config.txt.session", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt.session",
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
                  "ur0:tai/vita3g-disable.suprx\n"));
    CHECK(!platform_file_exists("ur0:tai/config.txt.bak"));
    CHECK(!platform_file_exists("ur0:tai/config.txt.1.bak"));
    CHECK(!platform_file_exists("ur0:tai/config.txt.2.bak"));

    /* A failed snapshot warns but does not lock the editor. */
    fake_reset();
    fake_fail_backup(1);
    editor_scan();
    CHECK(g_editor.backup_warning == 1);
    CHECK(g_editor.read_only == 0);
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(editor_next_action(row) != EDITOR_ACTION_NONE);
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);
    fake_fail_backup(0);
}

static void test_write_failures(void)
{
    char err[96];

    fake_reset();
    editor_scan();
    char before[32768];
    CHECK(read_file("ur0:tai/config.txt", before, (int)sizeof(before)) > 0);
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);

    /* The live write fails; the draft stays for a retry. */
    fake_fail_write_path("ur0:tai/config.txt", 1);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_FAILED);
    CHECK(strstr(err, "DATA") != NULL);
    CHECK(text_is("ur0:tai/config.txt", before));
    CHECK(!platform_file_exists("ur0:tai/config.txt.new"));
    CHECK(!platform_file_exists("ur0:tai/config.txt.old"));
    CHECK(g_editor.draft_dirty == 1);
    CHECK(g_editor.draft_count == 1);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    fake_fail_write_path("ur0:tai/config.txt", 0);

    /* A failed final move is recovered by the swap and still refuses. */
    fake_fail_rename_path("ur0:tai/config.txt", FAKE_RENAME_FINAL);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_FAILED);
    CHECK(strstr(err, "WRITE FAILED") != NULL);
    CHECK(strstr(err, "RENAME") != NULL);
    CHECK(text_is("ur0:tai/config.txt", before));
    CHECK(!platform_file_exists("ur0:tai/config.txt.new"));
    CHECK(!platform_file_exists("ur0:tai/config.txt.old"));
    CHECK(g_editor.draft_dirty == 1);
    fake_fail_rename_path("ur0:tai/config.txt", FAKE_RENAME_NONE);

    /* A snapshot that cannot be written refuses before the live write. */
    fake_fail_write_path("ur0:tai/config.txt.undo", 1);
    int writes = fake_write_count();
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_FAILED);
    CHECK(strstr(err, "UNDO") != NULL);
    CHECK(text_is("ur0:tai/config.txt", before));
    CHECK(fake_write_count() == writes + 1);
    CHECK(g_editor.draft_dirty == 1);
    fake_fail_write_path("ur0:tai/config.txt.undo", 0);

    /* The retained draft commits once the card accepts the write. */
    writes = fake_write_count();
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(fake_write_count() == writes + 2);
    CHECK(g_editor.draft_dirty == 0);
    CHECK(text_is("ur0:tai/config.txt.undo", before));
    CHECK(text_is("ur0:tai/config.txt", SAMPLE_OFF_KUBRIDGE));
}

static void test_safety_refusals(void)
{
    fake_reset();
    editor_scan();
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    char err[96];

    fake_add_file("ur0:tai/config.txt", "# changed\n");
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "CONFIG CHANGED") != NULL);

    char cfg[EDITOR_LINES_MAX * 24 + 64];
    size_t off = 0;
    off += (size_t)snprintf(cfg + off, sizeof(cfg) - off, "*KERNEL\n");
    for (int i = 0; i < EDITOR_LINES_MAX + 1; ++i)
        off += (size_t)snprintf(cfg + off, sizeof(cfg) - off,
                                "ur0:tai/p%03d.skprx\n", i);
    fake_reset();
    fake_add_file("ur0:tai/config.txt", cfg);
    editor_scan();
    CHECK(g_editor.lines_truncated == 1);
    CHECK(g_editor.read_only == 1);
    CHECK(strcmp(editor_readonly_reason(), "TOO MANY LINES") == 0);
    CHECK(editor_next_action(0) == EDITOR_ACTION_NONE);
    CHECK(stage_commit(0, 0, err, (int)sizeof(err)) != 0);

    fake_reset();
    fake_remove_file("ur0:tai/config.txt");
    editor_scan();
    CHECK(g_editor.source_ok == 0);
    CHECK(strcmp(editor_readonly_reason(), "NO CONFIG.TXT") == 0);

    fake_reset();
    fake_fail_read(1);
    editor_scan();
    CHECK(g_editor.unreadable == 1);
    CHECK(strcmp(editor_readonly_reason(), "CONFIG UNREADABLE") == 0);
    fake_fail_read(0);

    fake_reset();
    char long_cfg[256];
    char long_path[140];
    memset(long_path, 'a', 120);
    memcpy(long_path + 120, ".skprx", 7);
    long_path[127] = '\0';
    snprintf(long_cfg, sizeof(long_cfg), "*KERNEL\n%s\n", long_path);
    fake_add_file("ur0:tai/config.txt", long_cfg);
    editor_scan();
    CHECK(g_editor.paths_truncated == 1);
    CHECK(g_editor.lines_truncated == 0);
    CHECK(g_editor.read_only == 0);
    CHECK(g_editor.row_count == 0);
}

static void test_statuses(void)
{
    char err[96];

    /* A partial row disables first; a second press enables all lines. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\nur0:tai/kubridge.skprx\n"
                  "*NPXS10015\n#ur0:tai/kubridge.skprx\n");
    editor_scan();
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(g_editor.rows[row].status == EDITOR_PARTIAL);
    CHECK(editor_next_action(row) == 0);
    CHECK(g_editor.rows[row].active_lines == 1);
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_PARTIAL);
    CHECK(g_editor.rows[row].next_change == -1);
    CHECK(stage_commit(row, 1, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_LOADED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_PARTIAL);
    CHECK(g_editor.rows[row].next_change == 1);

    /* A missing file that is still active can be disabled. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*KERNEL\nur0:tai/absent.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/absent.skprx");
    CHECK(row >= 0);
    CHECK(g_editor.rows[row].status == EDITOR_MISSING);
    CHECK(editor_next_action(row) == 0);
    CHECK(g_editor.rows[row].present == 0);
    CHECK(g_editor.rows[row].active_lines == 1);
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_MISSING);
    CHECK(editor_next_action(row) == -1);
    CHECK(g_editor.rows[row].active_lines == 0);
    CHECK(g_editor.rows[row].reached_lines == 1);
    CHECK(stage_commit(row, 1, err, (int)sizeof(err)) != 0);
    CHECK(strstr(err, "FILE IS MISSING") != NULL);

    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*KERNEL\n#ur0:tai/absent.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/absent.skprx");
    CHECK(row >= 0);
    CHECK(g_editor.rows[row].status == EDITOR_MISSING);
    CHECK(editor_next_action(row) == -1);
    CHECK(g_editor.rows[row].reached_lines == 1);
    CHECK(g_editor.rows[row].present == 0);
    CHECK(g_editor.rows[row].active_lines == 0);

    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*KERNEL\nur0:tai/taihen.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/taihen.skprx");
    CHECK(g_editor.rows[row].status == EDITOR_BOOT);
    CHECK(editor_next_action(row) == -1);
    CHECK(g_editor.rows[row].boot == 1);

    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*KERNEL\nur0:tai/kubridge.skprx\n*!ALL\n"
                  "ur0:tai/extra.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/extra.skprx");
    CHECK(g_editor.rows[row].status == EDITOR_NOT_REACHED);
    CHECK(editor_next_action(row) == -1);
    CHECK(g_editor.rows[row].reached_lines == 0);

    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*main\n#ur0:tai/henkaku.suprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/henkaku.suprx");
    CHECK(g_editor.rows[row].locked == 1);
    CHECK(editor_next_action(row) == 1);
    CHECK(stage_commit(row, 1, err, (int)sizeof(err)) == 0);
    CHECK(editor_next_action(row) == -1);
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) != 0);
}

static void test_bom_and_invalid(void)
{
    char err[96];

    /* A BOM must not hide the first section or make the file read-only. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "\xEF\xBB\xBF*KERNEL\nur0:tai/kubridge.skprx\n");
    editor_scan();
    CHECK(g_editor.valid == 1);
    CHECK(g_editor.invalid == 0);
    CHECK(g_editor.read_only == 0);
    CHECK(g_editor.row_count == 1);
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(row >= 0);
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);

    char after[32768];
    CHECK(read_file("ur0:tai/config.txt", after, (int)sizeof(after)) > 0);
    CHECK((unsigned char)after[0] == 0xEF);
    CHECK((unsigned char)after[1] == 0xBB);
    CHECK((unsigned char)after[2] == 0xBF);
    CHECK(strstr(after, "#ur0:tai/kubridge.skprx") != NULL);

    /* A path before the first section is a warning, not a lock. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "ur0:tai/stray.skprx\n*KERNEL\nur0:tai/kubridge.skprx\n");
    editor_scan();
    CHECK(g_editor.invalid == 1);
    CHECK(g_editor.read_only == 0);
    CHECK(g_editor.row_count == 2);
    int stray = editor_row_for_path("ur0:tai/stray.skprx");
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(g_editor.rows[stray].status == EDITOR_NOT_REACHED);
    CHECK(editor_next_action(stray) == EDITOR_ACTION_NONE);
    CHECK(editor_next_action(row) != EDITOR_ACTION_NONE);
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);

    /* A prose comment ending in a plugin suffix is not a row. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "# see kubridge.skprx for the bridge\n*KERNEL\n"
                  "ur0:tai/kubridge.skprx\n");
    editor_scan();
    CHECK(g_editor.row_count == 1);
}

static void test_case_and_boot(void)
{
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*KERNEL\nur0:tai/KUBRIDGE.SKPRX\n");
    editor_scan();
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(row >= 0);
    CHECK(g_editor.rows[row].status == EDITOR_LOADED);
    CHECK(strcmp(g_editor.rows[row].name, "KUBRIDGE.SKPRX") == 0);
}

static void test_orphans(void)
{
    fake_reset();
    fake_add_file("ur0:tai/zzz.skprx", "");
    fake_add_file("ur0:tai/aaa.skprx", "");
    editor_scan();

    CHECK(!g_editor.orphans_unverified);
    CHECK(g_editor.orphan_count == 3);
    CHECK(strcmp(g_editor.orphans[0], "ur0:tai/aaa.skprx") == 0);
    CHECK(strcmp(g_editor.orphans[1], "ur0:tai/extra.skprx") == 0);
    CHECK(strcmp(g_editor.orphans[2], "ur0:tai/zzz.skprx") == 0);

    for (int i = 0; i < g_editor.orphan_count; ++i) {
        CHECK(!tai_ends_with(g_editor.orphans[i], ".original"));
        CHECK(!tai_ends_with(g_editor.orphans[i], ".session"));
        CHECK(!tai_ends_with(g_editor.orphans[i], ".bak"));
    }

    /* A missing ux0:tai directory is an empty listing, not an error. */
    CHECK(!platform_file_exists("ux0:tai"));
    CHECK(!g_editor.orphans_unverified);

    /* More files than the section can show is stated. */
    fake_reset();
    for (int i = 0; i < 40; ++i) {
        char path[64];
        snprintf(path, sizeof(path), "ur0:tai/overflow%02d.skprx", i);
        fake_add_file(path, "");
    }
    editor_scan();
    CHECK(g_editor.orphan_count == EDITOR_ORPHANS_MAX);
    CHECK(g_editor.orphan_truncated == 1);
}

static void test_ux0_precedence_and_empty(void)
{
    fake_reset();
    fake_add_file("ux0:tai/config.txt", "*main\nux0:tai/kubridge.skprx\n");
    fake_add_file("ux0:tai/kubridge.skprx", "");
    editor_scan();

    CHECK(strcmp(g_editor.source, "ux0:tai/config.txt") == 0);
    CHECK(strcmp(g_editor.ignored, "ur0:tai/config.txt") == 0);
    CHECK(g_editor.row_count == 1);
    CHECK(editor_row_for_path("ux0:tai/kubridge.skprx") >= 0);
    CHECK(editor_row_for_path("ur0:tai/kubridge.skprx") < 0);
    CHECK(text_is("ux0:tai/config.txt.session",
                  "*main\nux0:tai/kubridge.skprx\n"));

    fake_reset();
    fake_add_file("ux0:tai/config.txt", "");
    editor_scan();

    CHECK(strcmp(g_editor.source, "ux0:tai/config.txt") == 0);
    CHECK(g_editor.row_count == 0);
    CHECK(g_editor.valid == 1);
    CHECK(g_editor.read_only == 0);
    CHECK(text_is("ux0:tai/config.txt.session", ""));
}

/* The status badge reports the running set; a file edit is a second state. */
static void test_loaded_vs_next_boot(void)
{
    char err[96];

    fake_reset();
    editor_scan();
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(g_editor.rows[row].loaded == 1);
    CHECK(g_editor.rows[row].status == EDITOR_LOADED);
    CHECK(g_editor.rows[row].next_change == 0);

    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.rows[row].loaded == 1);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_LOADED);
    CHECK(g_editor.rows[row].next_change == -1);
    CHECK(strcmp(editor_status_text(g_editor.rows[row].launch_status),
                 "LOADED") == 0);

    CHECK(stage_commit(row, 1, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_LOADED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_LOADED);
    CHECK(g_editor.rows[row].next_change == 0);
    CHECK(g_editor.pending_count == 0);

    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);
    editor_scan();
    CHECK(g_editor.rows[row].loaded == 0);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_DISABLED);
    CHECK(g_editor.rows[row].next_change == 0);
    CHECK(g_editor.pending_count == 0);
    CHECK(g_editor.undo_available == 1);
}

static void test_locked_missing_agrees(void)
{
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\nur0:tai/henkaku.suprx\n#ur0:tai/henkaku.suprx\n");
    fake_remove_file("ur0:tai/henkaku.suprx");
    editor_scan();

    int row = editor_row_for_path("ur0:tai/henkaku.suprx");
    CHECK(row >= 0);
    CHECK(g_editor.rows[row].status == EDITOR_MISSING);
    CHECK(g_editor.rows[row].locked == 1);
    CHECK(editor_next_action(row) == -1);
    CHECK(g_editor.rows[row].reached_lines == 2);
    CHECK(g_editor.rows[row].present == 0);
}

static void test_interrupted_swap_recovery(void)
{
    fake_reset();
    fake_remove_file("ur0:tai/config.txt");
    fake_add_file("ur0:tai/config.txt.old", SAMPLE);

    editor_scan();
    CHECK(g_editor.recovered == 1);
    CHECK(g_editor.source_ok == 1);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(!platform_file_exists("ur0:tai/config.txt.old"));
    CHECK(g_editor.read_only == 0);
    CHECK(g_editor.row_count == 9);
}

static void test_undo_snapshot(void)
{
    char err[96];

    fake_reset();
    editor_scan();
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(g_editor.undo_available == 0);
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.undo_available == 1);
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE));

    editor_scan();
    CHECK(g_editor.rows[row].loaded == 0);
    CHECK(g_editor.pending_count == 0);
    CHECK(g_editor.undo_available == 1);

    CHECK(preview_commit(EDITOR_SNAPSHOT_UNDO, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(g_editor.rows[row].loaded == 0);
    CHECK(g_editor.rows[row].next_change == 1);
    CHECK(g_editor.undo_available == 0);

    /* An external edit invalidates the undo, and nothing is written. */
    fake_reset();
    editor_scan();
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);
    fake_add_file("ur0:tai/config.txt", "# changed elsewhere\n");
    CHECK(preview_commit(EDITOR_SNAPSHOT_UNDO, err, (int)sizeof(err)) != 0);
    CHECK(strstr(err, "CONFIG CHANGED") != NULL);
    CHECK(text_is("ur0:tai/config.txt", "# changed elsewhere\n"));

    fake_reset();
    editor_scan();
    CHECK(stage_commit(row, 0, err, (int)sizeof(err)) == 0);
    fake_remove_file("ur0:tai/config.txt");
    editor_scan();
    CHECK(g_editor.source_ok == 0);
    CHECK(preview_commit(EDITOR_SNAPSHOT_UNDO, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
}

static void test_scan_reads_undo_only(void)
{
    fake_reset();
    fake_add_file("ur0:tai/config.txt.undo", SAMPLE_OFF_KUBRIDGE);

    editor_scan();

    /* Scan derives availability from `.undo`; it never rewrites or removes it. */
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE_OFF_KUBRIDGE));
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_UNDO) == 1);
    CHECK(editor_undo_available() == 1);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));

    editor_scan();
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE_OFF_KUBRIDGE));
    CHECK(editor_undo_available() == 1);
}

static void test_preview_original(void)
{
    char err[160];

    fake_reset();
    fake_add_file("ur0:tai/config.txt.original", SAMPLE_OFF_KUBRIDGE);
    editor_scan();

    CHECK(text_is("ur0:tai/config.txt.original", SAMPLE_OFF_KUBRIDGE));
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_ORIGINAL) == 1);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_SESSION) == 1);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_UNDO) == 0);
    CHECK(editor_undo_available() == 0);

    int writes = fake_write_count();
    int stats = fake_stat_count();
    int reads = fake_backup_read_count();

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_ORIGINAL, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(err[0] == '\0');
    CHECK(fake_write_count() == writes);
    CHECK(fake_stat_count() == stats); /* every snapshot path is in the baseline */
    CHECK(fake_backup_read_count() == reads + 1); /* read exactly once */
    CHECK(g_editor.preview_active == 1);
    CHECK(editor_preview_active() == 1);
    CHECK(g_editor.preview_kind == EDITOR_SNAPSHOT_ORIGINAL);

    int kubridge = editor_row_for_path("ur0:tai/kubridge.skprx");
    int shellbat = editor_row_for_path("ur0:tai/shellbat.suprx");

    CHECK(kubridge >= 0);
    CHECK(shellbat >= 0);
    CHECK(editor_preview_diff(kubridge) == EDITOR_DIFF_DISABLE);
    CHECK(strcmp(editor_diff_text(editor_preview_diff(kubridge)),
                 "WILL DISABLE") == 0);
    CHECK(editor_preview_diff(shellbat) == EDITOR_DIFF_UNCHANGED);
    CHECK(editor_diff_text(EDITOR_DIFF_UNCHANGED) == NULL);
    CHECK(strcmp(editor_diff_text(EDITOR_DIFF_ENABLE), "WILL ENABLE") == 0);
    CHECK(strcmp(editor_diff_text(EDITOR_DIFF_REMOVE), "WILL REMOVE") == 0);
    CHECK(editor_preview_no_change() == 0);
    CHECK(editor_preview_locked_removal() == 0);
    CHECK(editor_preview_locked_path() == NULL);
    CHECK(editor_preview_removed_count() == 0);
    CHECK(editor_preview_removed_path(0) == NULL);
    CHECK(editor_preview_discards_draft() == 0);
    CHECK(strcmp(g_editor.text, SAMPLE_OFF_KUBRIDGE) == 0);
    CHECK(g_editor.rows[kubridge].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[kubridge].launch_status == EDITOR_LOADED);
    CHECK(g_editor.rows[kubridge].next_change == -1);
    CHECK(g_editor.draft_dirty == 0);
    CHECK(!platform_file_exists("ur0:tai/config.txt.undo"));

    editor_cancel_preview();
    CHECK(editor_preview_active() == 0);
    CHECK(g_editor.preview_active == 0);
    CHECK(strcmp(g_editor.text, SAMPLE) == 0);
    CHECK(g_editor.rows[kubridge].status == EDITOR_LOADED);
    CHECK(g_editor.rows[kubridge].launch_status == EDITOR_LOADED);
    CHECK(g_editor.pending_count == 0);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(fake_write_count() == writes);
}

static void test_preview_session_remove_and_add(void)
{
    char err[160];

    /* A working-set path the snapshot does not list is a removal. */
    fake_reset();
    editor_scan();
    fake_add_file("ur0:tai/config.txt.session", SAMPLE_NO_SHELLBAT);

    int writes = fake_write_count();
    int stats = fake_stat_count();
    int reads = fake_backup_read_count();

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(g_editor.preview_kind == EDITOR_SNAPSHOT_SESSION);
    CHECK(fake_backup_read_count() == reads + 1);
    CHECK(fake_stat_count() == stats); /* no baseline path is statted */
    CHECK(editor_preview_removed_count() == 1);
    CHECK(strcmp(editor_preview_removed_path(0),
                 "ur0:tai/shellbat.suprx") == 0);
    CHECK(editor_preview_removed_path(1) == NULL);
    CHECK(editor_preview_locked_removal() == 0);
    CHECK(editor_preview_locked_path() == NULL);
    CHECK(editor_preview_no_change() == 0);
    CHECK(editor_row_for_path("ur0:tai/shellbat.suprx") < 0);
    CHECK(fake_write_count() == writes);

    editor_cancel_preview();
    CHECK(editor_row_for_path("ur0:tai/shellbat.suprx") >= 0);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));

    /*
     * A path the working set lacks is proposed. The preview performs one
     * presence check for it (there is no scan-time fact), not one per plugin.
     */
    fake_add_file("ur0:tai/config.txt.session", SAMPLE_PLUS_EXTRA);
    stats = fake_stat_count();
    reads = fake_backup_read_count();
    writes = fake_write_count();

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(fake_backup_read_count() == reads + 1);
    CHECK(fake_stat_count() == stats + 1);

    int extra = editor_row_for_path("ur0:tai/extra.skprx");
    CHECK(extra >= 0);
    CHECK(editor_preview_diff(extra) == EDITOR_DIFF_ENABLE);
    CHECK(strcmp(editor_diff_text(editor_preview_diff(extra)),
                 "WILL ENABLE") == 0);
    CHECK(g_editor.rows[extra].status == EDITOR_LOADED);
    CHECK(g_editor.rows[extra].launch_status == EDITOR_DISABLED);
    CHECK(g_editor.rows[extra].loaded == 0);
    CHECK(g_editor.rows[extra].next_change == 1);
    CHECK(g_editor.rows[extra].present == 1);
    CHECK(editor_preview_removed_count() == 0);
    CHECK(editor_preview_no_change() == 0);
    CHECK(fake_write_count() == writes);

    editor_cancel_preview();
    CHECK(editor_row_for_path("ur0:tai/extra.skprx") < 0);
    CHECK(file_row_known("ur0:tai/extra.skprx") == 0);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
}

static void test_preview_draft_diff(void)
{
    char err[160];
    char warning[160];

    fake_reset();
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(g_editor.draft_dirty == 1);

    fake_add_file("ur0:tai/config.txt.session", SAMPLE);

    int writes = fake_write_count();

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_preview_discards_draft() == 1);
    CHECK(g_editor.preview_saved_dirty == 1);
    /*
     * The draft disabled kubridge, so the file-state snapshot enables it. A
     * diff against the file baseline would wrongly report no change.
     */
    CHECK(editor_preview_diff(row) == EDITOR_DIFF_ENABLE);
    CHECK(editor_preview_no_change() == 0);
    CHECK(fake_write_count() == writes);

    plugin_flow_dock_warning(NULL, warning, (int)sizeof(warning));
    CHECK(strcmp(warning, "DRAFT WILL BE DISCARDED") == 0);
    CHECK(strcmp(plugin_flow_action_label(PLUGIN_FLOW_PREVIEW,
                                          PLUGIN_DOCK_CONFIRM),
                 "DISCARD DRAFT & RESTORE") == 0);

    editor_cancel_preview();
    CHECK(editor_preview_active() == 0);
    CHECK(strcmp(g_editor.text, SAMPLE_OFF_KUBRIDGE) == 0);
    CHECK(g_editor.draft_dirty == 1);
    CHECK(editor_draft_dirty() == 1);
    CHECK(editor_draft_count() == 1);
    CHECK(editor_draft_change(row) == -1);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_LOADED);
    CHECK(g_editor.rows[row].next_change == -1);
    CHECK(fake_write_count() == writes);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
}

static void test_preview_restore_commit(void)
{
    char err[160];

    fake_reset();
    editor_scan();

    int kernel = editor_row_for_path("ur0:tai/kubridge.skprx");
    int shell = editor_row_for_path("ur0:tai/shellbat.suprx");

    CHECK(editor_stage_toggle(kernel, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    fake_add_file("ur0:tai/config.txt.session", SAMPLE_SHELLBAT_ON);

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_preview_discards_draft() == 1);
    CHECK(editor_preview_diff(kernel) == EDITOR_DIFF_ENABLE);
    CHECK(editor_preview_diff(shell) == EDITOR_DIFF_ENABLE);

    int writes = fake_write_count();
    int reads = fake_backup_read_count();

    CHECK(editor_commit_preview(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(err[0] == '\0');
    CHECK(fake_write_count() == writes + 2);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE_SHELLBAT_ON));
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE));
    CHECK(!platform_file_exists("ur0:tai/config.txt.new"));
    CHECK(!platform_file_exists("ur0:tai/config.txt.old"));
    CHECK(editor_preview_active() == 0);
    CHECK(g_editor.preview_active == 0);
    CHECK(g_editor.draft_dirty == 0);
    CHECK(editor_draft_count() == 0);
    CHECK(g_editor.pending_count == 1);
    CHECK(editor_undo_available() == 1);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_ORIGINAL) == 1);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_SESSION) == 1);
    CHECK(strcmp(g_editor.last_text, SAMPLE_SHELLBAT_ON) == 0);
    CHECK(g_editor.rows[kernel].status == EDITOR_LOADED);
    CHECK(g_editor.rows[kernel].launch_status == EDITOR_LOADED);
    CHECK(g_editor.rows[kernel].next_change == 0);
    CHECK(g_editor.rows[shell].status == EDITOR_LOADED);
    CHECK(g_editor.rows[shell].launch_status == EDITOR_DISABLED);
    CHECK(g_editor.rows[shell].next_change == 1);
    CHECK(editor_preview_diff(kernel) == EDITOR_DIFF_UNCHANGED);

    /* The restore updated the cached facts without touching the card again. */
    reads = fake_backup_read_count();
    for (int frame = 0; frame < 2; ++frame) {
        CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_ORIGINAL) == 1);
        CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_SESSION) == 1);
        CHECK(editor_undo_available() == 1);
    }
    CHECK(fake_backup_read_count() == reads);
}

static void test_preview_restore_failure_and_stale(void)
{
    char err[160];

    fake_reset();
    editor_scan();
    fake_add_file("ur0:tai/config.txt.session", SAMPLE_OFF_KUBRIDGE);

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);

    int writes = fake_write_count();

    fake_fail_write_path("ur0:tai/config.txt", 1);
    CHECK(editor_commit_preview(err, (int)sizeof(err)) ==
          EDITOR_WRITE_FAILED);
    CHECK(strstr(err, "DATA") != NULL);
    CHECK(fake_write_count() == writes + 2); /* .undo, then the failed live */
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE));
    CHECK(editor_preview_active() == 1);
    CHECK(strcmp(g_editor.text, SAMPLE_OFF_KUBRIDGE) == 0);
    /* The pre-write snapshot equals the recovered live file: no undo gap. */
    CHECK(editor_undo_available() == 0);
    fake_fail_write_path("ur0:tai/config.txt", 0);

    CHECK(editor_commit_preview(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE_OFF_KUBRIDGE));
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE));
    CHECK(editor_undo_available() == 1);
    CHECK(editor_preview_active() == 0);

    /* A file changed behind the preview refuses and keeps the preview. */
    fake_reset();
    editor_scan();
    fake_add_file("ur0:tai/config.txt.session", SAMPLE_OFF_KUBRIDGE);
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);

    fake_add_file("ur0:tai/config.txt", "# changed elsewhere\n");
    writes = fake_write_count();

    CHECK(editor_commit_preview(err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "CONFIG CHANGED") != NULL);
    CHECK(fake_write_count() == writes);
    CHECK(text_is("ur0:tai/config.txt", "# changed elsewhere\n"));
    CHECK(editor_preview_active() == 1);

    editor_cancel_preview();
    CHECK(editor_preview_active() == 0);
    CHECK(strcmp(g_editor.text, SAMPLE) == 0);
}

static void test_preview_locked_removal(void)
{
    char err[160];

    /* The locked HENKAKU path may be listed, but never confirmed away. */
    fake_reset();
    editor_scan();
    fake_add_file("ur0:tai/config.txt.session", SAMPLE_NO_HENKAKU);

    int writes = fake_write_count();

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_preview_removed_count() == 1);
    CHECK(strcmp(editor_preview_removed_path(0),
                 "ur0:tai/henkaku.suprx") == 0);
    CHECK(editor_preview_locked_removal() == 1);
    CHECK(strcmp(editor_preview_locked_path(),
                 "ur0:tai/henkaku.suprx") == 0);
    CHECK(editor_preview_no_change() == 0);

    CHECK(editor_commit_preview(err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "RESTORE DROPS") != NULL);
    CHECK(strstr(err, "henkaku.suprx") != NULL);
    CHECK(fake_write_count() == writes);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(editor_preview_active() == 1);
    editor_cancel_preview();

    /* A BOOT module is refused the same way and named. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*KERNEL\nur0:tai/taihen.skprx\n"
                  "ur0:tai/kubridge.skprx\n");
    editor_scan();

    int taihen = editor_row_for_path("ur0:tai/taihen.skprx");
    CHECK(taihen >= 0);
    CHECK(g_editor.rows[taihen].boot == 1);

    fake_add_file("ur0:tai/config.txt.session",
                  "*KERNEL\nur0:tai/kubridge.skprx\n");
    writes = fake_write_count();

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_preview_locked_removal() == 1);
    CHECK(strcmp(editor_preview_locked_path(),
                 "ur0:tai/taihen.skprx") == 0);
    CHECK(editor_commit_preview(err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "RESTORE DROPS") != NULL);
    CHECK(strstr(err, "taihen.skprx") != NULL);
    CHECK(fake_write_count() == writes);
    CHECK(editor_preview_active() == 1);
    editor_cancel_preview();
}

static void test_preview_locked_disabled(void)
{
    char err[160];

    /*
     * HENKAKU stays in the file but every reached line is commented out. It is
     * not a textual removal, but it is a functional one and must not bypass the
     * locked-row rule.
     */
    fake_reset();
    editor_scan();
    fake_add_file("ur0:tai/config.txt.session", SAMPLE_HENKAKU_OFF);

    int henkaku = editor_row_for_path("ur0:tai/henkaku.suprx");
    int writes = fake_write_count();

    CHECK(henkaku >= 0);
    CHECK(g_editor.rows[henkaku].status == EDITOR_LOADED);

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_preview_active() == 1);

    henkaku = editor_row_for_path("ur0:tai/henkaku.suprx");
    CHECK(henkaku >= 0);
    CHECK(g_editor.rows[henkaku].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[henkaku].active_lines == 0);
    CHECK(g_editor.rows[henkaku].reached_lines > 0);

    /* Still present in the file: not a WILL REMOVE, but a protected drop. */
    CHECK(editor_preview_removed_count() == 0);
    CHECK(editor_preview_locked_removal() == 1);
    CHECK(strcmp(editor_preview_locked_path(),
                 "ur0:tai/henkaku.suprx") == 0);
    CHECK(editor_preview_no_change() == 0);

    CHECK(editor_commit_preview(err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "RESTORE DROPS") != NULL);
    CHECK(strstr(err, "henkaku.suprx") != NULL);
    CHECK(fake_write_count() == writes);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(editor_preview_active() == 1);

    editor_cancel_preview();
    CHECK(editor_preview_active() == 0);
    CHECK(strcmp(g_editor.text, SAMPLE) == 0);
    henkaku = editor_row_for_path("ur0:tai/henkaku.suprx");
    CHECK(g_editor.rows[henkaku].status == EDITOR_LOADED);

    /* Cancel also restores a draft, not just the file state. */
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_preview_locked_removal() == 1);
    CHECK(editor_preview_discards_draft() == 1);

    writes = fake_write_count();

    CHECK(editor_commit_preview(err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(fake_write_count() == writes);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));

    editor_cancel_preview();
    CHECK(strcmp(g_editor.text, SAMPLE_OFF_KUBRIDGE) == 0);
    CHECK(g_editor.draft_dirty == 1);
    CHECK(editor_draft_change(editor_row_for_path("ur0:tai/kubridge.skprx")) ==
          -1);

    henkaku = editor_row_for_path("ur0:tai/henkaku.suprx");
    CHECK(g_editor.rows[henkaku].status == EDITOR_LOADED);
}

/*
 * A `*!` halt leaves the locked path in the file but unreached. That is a
 * functional removal too, and the dock must name it and refuse the commit.
 */
static void test_preview_locked_halt(void)
{
    char err[160];

    fake_reset();
    editor_scan();
    fake_add_file("ur0:tai/config.txt.session", SAMPLE_HENKAKU_HALTED);

    int henkaku = editor_row_for_path("ur0:tai/henkaku.suprx");
    int writes = fake_write_count();

    CHECK(henkaku >= 0);
    CHECK(g_editor.rows[henkaku].status == EDITOR_LOADED);

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);

    henkaku = editor_row_for_path("ur0:tai/henkaku.suprx");
    CHECK(henkaku >= 0);
    CHECK(g_editor.rows[henkaku].status == EDITOR_NOT_REACHED);
    CHECK(g_editor.rows[henkaku].active_lines == 0);
    CHECK(g_editor.rows[henkaku].reached_lines == 0);
    CHECK(editor_preview_diff(henkaku) == EDITOR_DIFF_DISABLE);
    CHECK(editor_preview_removed_count() == 0);
    CHECK(editor_preview_locked_removal() == 1);
    CHECK(strcmp(editor_preview_locked_path(),
                 "ur0:tai/henkaku.suprx") == 0);

    CHECK(editor_commit_preview(err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "RESTORE DROPS") != NULL);
    CHECK(strstr(err, "henkaku.suprx") != NULL);
    CHECK(fake_write_count() == writes);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));

    editor_cancel_preview();
    CHECK(editor_preview_active() == 0);
}

static void test_preview_no_change(void)
{
    char err[160];

    fake_reset();
    editor_scan();
    fake_add_file("ur0:tai/config.txt.session", SAMPLE);

    int writes = fake_write_count();

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_preview_no_change() == 1);
    CHECK(editor_preview_removed_count() == 0);
    CHECK(editor_preview_locked_removal() == 0);
    CHECK(editor_preview_diff(0) == EDITOR_DIFF_UNCHANGED);

    CHECK(editor_commit_preview(err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "NO CHANGES") != NULL);
    CHECK(fake_write_count() == writes);
    CHECK(editor_preview_active() == 1);
    editor_cancel_preview();

    /* A comment-only difference has the same plugin set: nothing to restore. */
    fake_add_file("ur0:tai/config.txt.session", "# different comment\n" SAMPLE);
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_preview_no_change() == 1);
    CHECK(editor_commit_preview(err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(fake_write_count() == writes);
    editor_cancel_preview();
}

/*
 * A restore can drop a loaded, non-protected path. The launch set still holds
 * it, so the dock stays SAVED and offers REBOOT NOW (R36).
 */
static void test_restore_removal_pending(void)
{
    char err[160];

    fake_reset();
    metrics_init();
    editor_scan();
    fake_add_file("ur0:tai/config.txt.session", SAMPLE_NO_LIBSHACCCG);

    int writes = fake_write_count();

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_preview_removed_count() == 1);
    CHECK(strcmp(editor_preview_removed_path(0),
                 "ur0:tai/libshacccg.suprx") == 0);

    CHECK(editor_commit_preview(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(fake_write_count() == writes + 2);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE_NO_LIBSHACCCG));
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE));
    CHECK(g_editor.pending_count == 1);
    CHECK(g_editor.loaded_total == 7);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_SAVED);
    CHECK(plugin_flow_action_available(PLUGIN_FLOW_SAVED,
                                       PLUGIN_DOCK_REBOOT) == 1);
    CHECK(editor_row_for_path("ur0:tai/libshacccg.suprx") < 0);
}

/*
 * A live config the parser could not read is not a restore target: there is no
 * usable pre-restore `.undo`, and the bytes being replaced were never seen.
 * Undo remains the recovery path.
 */
static void test_restore_unreadable_refused(void)
{
    char err[160];

    fake_reset();
    fake_add_file("ur0:tai/config.txt.original", SAMPLE_OFF_KUBRIDGE);
    fake_add_file("ur0:tai/config.txt.undo", SAMPLE_OFF_KUBRIDGE);
    editor_scan();
    CHECK(editor_can_restore() == 1);

    fake_fail_read_path("ur0:tai/config.txt", 1);
    editor_scan();

    CHECK(g_editor.unreadable == 1);
    CHECK(g_editor.source_ok == 0);
    CHECK(editor_can_restore() == 0);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_ORIGINAL) == 1);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_UNDO) == 1);

    /*
     * The cached state still blocks the restore; the card is readable again
     * only so the assertions can inspect what the later undo writes.
     */
    fake_fail_read_path("ur0:tai/config.txt", 0);

    int writes = fake_write_count();

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_ORIGINAL, err,
                                 (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "UNREADABLE") != NULL);
    CHECK(editor_preview_active() == 0);
    CHECK(fake_write_count() == writes);

    /* Undo is the D70 recovery path and still opens and commits. */
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_UNDO, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_commit_preview(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE_OFF_KUBRIDGE));
    CHECK(g_editor.unreadable == 0);
    CHECK(g_editor.write_unrecovered == 0);
    CHECK(!platform_file_exists("ur0:tai/config.txt.undo"));
}

/*
 * A failed save must not consume the previous one-step undo: the config did
 * not change, so the previous restore point is still the right one.
 */
static void test_commit_preserves_previous_undo(void)
{
    char err[160];

    fake_reset();
    editor_scan();

    int kernel = editor_row_for_path("ur0:tai/kubridge.skprx");
    int shell = editor_row_for_path("ur0:tai/shellbat.suprx");

    CHECK(editor_stage_toggle(kernel, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE));

    CHECK(editor_stage_toggle(shell, 1, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);

    int writes = fake_write_count();

    fake_fail_write_path("ur0:tai/config.txt", 1);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) ==
          EDITOR_WRITE_FAILED);
    CHECK(strstr(err, "DATA") != NULL);

    /* The new `.undo` was rolled back to the previous restore point. */
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt", SAMPLE_OFF_KUBRIDGE));
    CHECK(g_editor.undo_available == 1);
    CHECK(g_editor.draft_dirty == 1);
    CHECK(fake_write_count() >= writes + 3); /* .undo, failed live, rollback */

    fake_fail_write_path("ur0:tai/config.txt", 0);
}

/*
 * A verified undo removes `.undo`. If the removal fails, the dock must keep
 * reporting it: claiming the card is clean while the old bytes remain would
 * turn the one-step undo into a redo.
 */
static void test_undo_remove_failure(void)
{
    char err[160];

    fake_reset();
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE));

    fake_fail_remove_path("ur0:tai/config.txt.undo", 1);
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_UNDO, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_commit_preview(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(platform_file_exists("ur0:tai/config.txt.undo"));
    CHECK(editor_undo_available() == 1);

    fake_fail_remove_path("ur0:tai/config.txt.undo", 0);
}

static void test_preview_layout_launch(void)
{
    char err[160];

    fake_reset();
    editor_scan();

    int loaded = g_editor.loaded_total;

    fake_add_file("ur0:tai/config.txt.session", LAYOUT_SNAPSHOT);

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);

    int kubridge = editor_row_for_path("ur0:tai/kubridge.skprx");
    int shellbat = editor_row_for_path("ur0:tai/shellbat.suprx");
    int mystery = editor_row_for_path("ur0:tai/mystery.suprx");

    CHECK(kubridge >= 0);
    CHECK(shellbat >= 0);
    CHECK(mystery >= 0);
    CHECK(g_editor.rows[kubridge].launch_status == EDITOR_LOADED);
    CHECK(g_editor.rows[kubridge].status == EDITOR_LOADED);
    CHECK(g_editor.rows[kubridge].next_change == 0);
    CHECK(g_editor.rows[shellbat].launch_status == EDITOR_DISABLED);
    CHECK(g_editor.rows[shellbat].status == EDITOR_LOADED);
    CHECK(g_editor.rows[shellbat].next_change == 1);
    CHECK(g_editor.rows[mystery].launch_status == EDITOR_DISABLED);
    CHECK(g_editor.rows[mystery].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[mystery].next_change == 0);
    CHECK(g_editor.loaded_total == loaded);
    CHECK(g_editor.pending_count == 1);
    CHECK(file_launch_active("ur0:tai/kubridge.skprx") == 1);
    CHECK(file_launch_active("ur0:tai/shellbat.suprx") == 0);

    editor_cancel_preview();
    CHECK(g_editor.loaded_total == loaded);
    CHECK(g_editor.pending_count == 0);

    kubridge = editor_row_for_path("ur0:tai/kubridge.skprx");
    shellbat = editor_row_for_path("ur0:tai/shellbat.suprx");
    CHECK(g_editor.rows[kubridge].launch_status == EDITOR_LOADED);
    CHECK(g_editor.rows[shellbat].launch_status == EDITOR_DISABLED);
}

static void test_preview_undo_commit(void)
{
    char err[160];

    fake_reset();
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_undo_available() == 1);

    int writes = fake_write_count();
    int reads = fake_backup_read_count();

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_UNDO, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(fake_backup_read_count() == reads + 1);
    CHECK(g_editor.preview_kind == EDITOR_SNAPSHOT_UNDO);
    CHECK(editor_preview_diff(row) == EDITOR_DIFF_ENABLE);
    CHECK(editor_preview_no_change() == 0);
    CHECK(editor_preview_discards_draft() == 0);

    CHECK(editor_commit_preview(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(err[0] == '\0');
    CHECK(fake_write_count() == writes + 1); /* one live write, no .undo write */
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(!platform_file_exists("ur0:tai/config.txt.undo"));
    CHECK(editor_undo_available() == 0);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_UNDO) == 0);
    CHECK(editor_preview_active() == 0);
    CHECK(strcmp(g_editor.text, SAMPLE) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_LOADED);
    CHECK(g_editor.rows[row].launch_status == EDITOR_LOADED);
    CHECK(g_editor.rows[row].next_change == 0);
    CHECK(g_editor.pending_count == 0);

    reads = fake_backup_read_count();
    for (int frame = 0; frame < 2; ++frame) {
        CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_ORIGINAL) == 1);
        CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_SESSION) == 1);
        CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_UNDO) == 0);
        CHECK(editor_undo_available() == 0);
    }
    CHECK(fake_backup_read_count() == reads);
}

static void test_preview_undo_failure(void)
{
    char err[160];

    fake_reset();
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_UNDO, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);

    int writes = fake_write_count();

    fake_fail_write_path("ur0:tai/config.txt", 1);
    CHECK(editor_commit_preview(err, (int)sizeof(err)) ==
          EDITOR_WRITE_FAILED);
    CHECK(fake_write_count() == writes + 1); /* no .undo rewrite */
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt", SAMPLE_OFF_KUBRIDGE));
    CHECK(editor_preview_active() == 1);
    CHECK(editor_undo_available() == 1);
    fake_fail_write_path("ur0:tai/config.txt", 0);

    CHECK(editor_commit_preview(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(fake_write_count() == writes + 2);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(!platform_file_exists("ur0:tai/config.txt.undo"));
    CHECK(editor_undo_available() == 0);
    CHECK(editor_preview_active() == 0);
}

static void test_preview_undo_no_live(void)
{
    char err[160];

    fake_reset();
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    fake_remove_file("ur0:tai/config.txt");

    editor_scan();
    CHECK(g_editor.source_ok == 0);
    CHECK(strcmp(g_editor.source, "ur0:tai/config.txt") == 0);
    CHECK(editor_undo_available() == 1);

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_UNDO, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_preview_no_change() == 0);

    int writes = fake_write_count();

    CHECK(editor_commit_preview(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(fake_write_count() == writes + 1); /* no live file to snapshot */
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(!platform_file_exists("ur0:tai/config.txt.undo"));
    CHECK(g_editor.source_ok == 1);
    CHECK(g_editor.read_only == 0);
    CHECK(editor_undo_available() == 0);
    CHECK(g_editor.draft_dirty == 0);
    CHECK(g_editor.rows[row].status == EDITOR_LOADED);
}

static void test_preview_undo_no_live_stale(void)
{
    char err[160];

    fake_reset();
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    fake_remove_file("ur0:tai/config.txt");

    editor_scan();
    CHECK(g_editor.source_ok == 0);
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_UNDO, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);

    /* A file that appeared after the empty baseline is just as stale. */
    fake_add_file("ur0:tai/config.txt", "# appeared elsewhere\n");

    int writes = fake_write_count();

    CHECK(editor_commit_preview(err, (int)sizeof(err)) ==
          EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "CONFIG CHANGED") != NULL);
    CHECK(fake_write_count() == writes);
    CHECK(text_is("ur0:tai/config.txt", "# appeared elsewhere\n"));
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE));
    CHECK(editor_preview_active() == 1);
    CHECK(editor_undo_available() == 1);

    editor_cancel_preview();
}

static void test_preview_commit_added_path(void)
{
    char err[160];

    fake_reset();
    editor_scan();
    fake_add_file("ur0:tai/config.txt.session", SAMPLE_PLUS_EXTRA);

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);

    int extra = editor_row_for_path("ur0:tai/extra.skprx");

    CHECK(extra >= 0);
    CHECK(editor_preview_diff(extra) == EDITOR_DIFF_ENABLE);

    CHECK(editor_commit_preview(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE_PLUS_EXTRA));
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE));
    CHECK(file_row_known("ur0:tai/extra.skprx") == 1);

    extra = editor_row_for_path("ur0:tai/extra.skprx");
    CHECK(extra >= 0);
    CHECK(g_editor.rows[extra].status == EDITOR_LOADED);
    CHECK(g_editor.rows[extra].launch_status == EDITOR_DISABLED);
    CHECK(g_editor.rows[extra].loaded == 0);
    CHECK(g_editor.rows[extra].next_change == 1);
    CHECK(g_editor.draft_dirty == 0);

    /* The adopted path is a normal baseline row on the next edit. */
    CHECK(editor_stage_toggle(extra, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_draft_change(extra) == -1);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt.undo", SAMPLE_PLUS_EXTRA));
    CHECK(g_editor.rows[extra].launch_status == EDITOR_DISABLED);
    CHECK(g_editor.rows[extra].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[extra].next_change == 0);
}

static void test_preview_snapshot_unavailable(void)
{
    char err[160];

    fake_reset();
    editor_scan();

    /* The elapsed scan cached availability; a missing file is discovered then. */
    fake_remove_file("ur0:tai/config.txt.original");
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_ORIGINAL) == 1);

    int writes = fake_write_count();
    int reads = fake_backup_read_count();

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_ORIGINAL, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "NO RESTORE POINT") != NULL);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_ORIGINAL) == 0);
    CHECK(editor_preview_active() == 0);
    CHECK(fake_write_count() == writes);
    CHECK(fake_backup_read_count() == reads + 1);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(strcmp(g_editor.text, SAMPLE) == 0);

    /* An unreadable snapshot is unavailable too, and no preview is left open. */
    fake_add_file("ur0:tai/config.txt.session", SAMPLE_OFF_KUBRIDGE);
    fake_fail_read_path("ur0:tai/config.txt.session", 1);

    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_SESSION) == 1);
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "NO RESTORE POINT") != NULL);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_SESSION) == 0);
    CHECK(editor_preview_active() == 0);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    fake_fail_read_path("ur0:tai/config.txt.session", 0);

    /* A later successful read refreshes the cached fact. */
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_SESSION) == 1);
    editor_cancel_preview();

    /*
     * A snapshot the line model cannot represent fails closed: an incomplete
     * diff cannot prove the locked-path safeguard.
     */
    char big[EDITOR_LINES_MAX * 3 + 8];
    size_t off = 0;

    for (int i = 0; i <= EDITOR_LINES_MAX; ++i)
        off += (size_t)snprintf(big + off, sizeof(big) - off, "#\n");

    fake_add_file("ur0:tai/config.txt.session", big);
    writes = fake_write_count();

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_REFUSED);
    CHECK(strstr(err, "TOO COMPLEX") != NULL);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_SESSION) == 0);
    CHECK(editor_preview_active() == 0);
    CHECK(fake_write_count() == writes);
    CHECK(strcmp(g_editor.text, SAMPLE) == 0);
}

static void test_preview_availability_cached(void)
{
    fake_reset();
    editor_scan();

    CHECK(fake_backup_read_count() >= 3);

    int reads = fake_backup_read_count();

    for (int frame = 0; frame < 2; ++frame) {
        CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_ORIGINAL) == 1);
        CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_SESSION) == 1);
        CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_UNDO) == 0);
        CHECK(editor_undo_available() == 0);
    }

    CHECK(fake_backup_read_count() == reads);

    /* A save updates the cached undo fact; rendering it still reads nothing. */
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    char err[96];

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);

    reads = fake_backup_read_count();

    for (int frame = 0; frame < 2; ++frame) {
        CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_ORIGINAL) == 1);
        CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_SESSION) == 1);
        CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_UNDO) == 1);
        CHECK(editor_undo_available() == 1);
    }

    CHECK(fake_backup_read_count() == reads);
}

static void test_ux0_unreadable_no_fallback(void)
{
    char err[96];

    fake_reset();
    fake_add_file("ux0:tai/config.txt", "*main\nux0:tai/kubridge.skprx\n");
    fake_fail_read_path("ux0:tai/config.txt", 1);

    editor_scan();
    CHECK(strcmp(g_editor.source, "ux0:tai/config.txt") == 0);
    CHECK(g_editor.source_ok == 0);
    CHECK(g_editor.unreadable == 1);
    CHECK(g_editor.read_only == 1);
    CHECK(g_editor.row_count == 0);

    /* A ur0: undo copy must not steal the source from an unreadable ux0:. */
    fake_add_file("ur0:tai/config.txt.undo", SAMPLE);
    editor_scan();
    CHECK(strcmp(g_editor.source, "ux0:tai/config.txt") == 0);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(preview_commit(EDITOR_SNAPSHOT_UNDO, err, (int)sizeof(err)) != 0);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));

    fake_fail_read_path("ux0:tai/config.txt", 0);
}

/* The fake models the swap; platform_psp2.c is not linked into host tests. */
static void test_restore_stage_fake_model(void)
{
    char err[96];

    fake_reset();
    editor_scan();
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);

    fake_fail_rename_path("ur0:tai/config.txt",
                          FAKE_RENAME_FINAL | FAKE_RENAME_RESTORE);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) ==
          EDITOR_WRITE_UNRECOVERED);
    CHECK(strstr(err, "RESTORE") != NULL);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(g_editor.rows[row].next_change == -1);
    CHECK(g_editor.pending_count == 1);
    CHECK(g_editor.draft_dirty == 1);
    CHECK(g_editor.draft_count == 1);
    CHECK(!platform_file_exists("ur0:tai/config.txt"));
    CHECK(text_is("ur0:tai/config.txt.old", SAMPLE));
    CHECK(g_editor.write_unrecovered == 1);
    CHECK(editor_can_restore() == 0);

    /* The repair state must offer UNDO, not dead SAVE/CONFIRM buttons. */
    CHECK(plugin_flow_state() == PLUGIN_FLOW_LAST_WRITE);
    CHECK(plugin_flow_action_available(PLUGIN_FLOW_LAST_WRITE,
                                       PLUGIN_DOCK_UNDO) == 1);
    CHECK(editor_next_action(row) == EDITOR_ACTION_NONE);

    fake_fail_rename_path("ur0:tai/config.txt", FAKE_RENAME_NONE);

    /*
     * The live file is gone and the in-memory baseline equals `.undo`. The
     * advertised undo must still open and repair the card (D70), not report
     * NO CHANGES because the working set happens to match.
     */
    editor_discard_draft();
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_UNDO, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_preview_no_change() == 0);
    CHECK(editor_commit_preview(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(g_editor.write_unrecovered == 0);
    CHECK(!platform_file_exists("ur0:tai/config.txt.undo"));
}

static void test_count_parity(void)
{
    fake_reset();
    fake_add_file("ux0:tai/config.txt", "*main\nux0:tai/kubridge.skprx\n");
    fake_add_file("ux0:tai/kubridge.skprx", "");
    editor_scan();
    plugins_scan();

    CHECK(strcmp(g_editor.source, "ux0:tai/config.txt") == 0);
    CHECK(g_editor.row_count == 1);
    CHECK(editor_row_for_path("ux0:tai/kubridge.skprx") >= 0);
    CHECK(g_editor.loaded_total == g_plugins.count);
    CHECK(g_plugins.count == 1);
}

static int float_near(float a, float b)
{
    float delta = a > b ? a - b : b - a;

    return delta < 0.001f;
}

static void flow_fixture(UiState *ui)
{
    fake_reset();
    metrics_init();
    editor_scan();

    memset(ui, 0, sizeof(*ui));
    ui->tab = UI_TAB_PLUGINS;
    ui->selection = g_editor.row_count > 0 ? 0 : -1;
    ui->focus = UI_FOCUS_LIST;
}

static PluginDockAction dock_action(const UiState *ui)
{
    return plugin_flow_action_at(plugin_flow_state(), ui->dock_choice);
}

/*
 * Down from the list focuses the safe action, Left/Right walks the available
 * actions and clamps, and Up/Circle (focus_list) returns to the list.
 */
/*
 * The event routing that used to live only in app.c: Down past the last row
 * enters the dock, Up/Circle leaves, Left/Right moves, Triangle toggles, and
 * the modal owns accept/cancel. SAVE and CONFIRM map to different commit modes.
 */
static void test_plugin_nav(void)
{
    UiState ui;

    memset(&ui, 0, sizeof(ui));
    ui.tab = UI_TAB_HOME;

    CHECK(plugin_nav_action(&ui, INPUT_UP, 9, 0) ==
          PLUGIN_NAV_HOME_SCROLL_UP);
    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 9, 0) ==
          PLUGIN_NAV_HOME_SCROLL_DOWN);
    CHECK(plugin_nav_action(&ui, INPUT_ACCEPT, 9, 0) == PLUGIN_NAV_NONE);
    CHECK(plugin_nav_action(&ui, INPUT_TAB_LEFT, 9, 0) ==
          PLUGIN_NAV_TAB_TOGGLE);

    ui.tab = UI_TAB_PLUGINS;
    ui.focus = UI_FOCUS_LIST;
    ui.selection = 0;

    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 9, 0) ==
          PLUGIN_NAV_SELECT_DOWN);
    CHECK(plugin_nav_action(&ui, INPUT_UP, 9, 0) == PLUGIN_NAV_NONE);

    ui.selection = 8;
    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 9, 0) ==
          PLUGIN_NAV_DOCK_ENTER);
    CHECK(plugin_nav_action(&ui, INPUT_ACCEPT, 9, 0) ==
          PLUGIN_NAV_ACTIVATE_ROW);
    CHECK(plugin_nav_action(&ui, INPUT_OPTIONS, 9, 0) ==
          PLUGIN_NAV_DOCK_TOGGLE);

    ui.focus = UI_FOCUS_DOCK;
    CHECK(plugin_nav_action(&ui, INPUT_UP, 9, 0) == PLUGIN_NAV_DOCK_LEAVE);
    CHECK(plugin_nav_action(&ui, INPUT_CANCEL, 9, 0) ==
          PLUGIN_NAV_DOCK_LEAVE);
    CHECK(plugin_nav_action(&ui, INPUT_LEFT, 9, 0) == PLUGIN_NAV_DOCK_LEFT);
    CHECK(plugin_nav_action(&ui, INPUT_RIGHT, 9, 0) ==
          PLUGIN_NAV_DOCK_RIGHT);
    CHECK(plugin_nav_action(&ui, INPUT_ACCEPT, 9, 0) ==
          PLUGIN_NAV_ACTIVATE_DOCK);
    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 9, 0) == PLUGIN_NAV_NONE);

    /* The preview is read-only: Down enters the dock from anywhere. */
    ui.focus = UI_FOCUS_LIST;
    ui.selection = 0;
    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 9, 1) ==
          PLUGIN_NAV_DOCK_ENTER);
    CHECK(plugin_nav_action(&ui, INPUT_UP, 9, 1) == PLUGIN_NAV_NONE);

    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 0, 0) ==
          PLUGIN_NAV_DOCK_ENTER);

    ui.modal = UI_MODAL_ERROR;
    CHECK(plugin_nav_action(&ui, INPUT_ACCEPT, 9, 0) ==
          PLUGIN_NAV_MODAL_DISMISS);
    CHECK(plugin_nav_action(&ui, INPUT_CANCEL, 9, 0) ==
          PLUGIN_NAV_MODAL_DISMISS);
    CHECK(plugin_nav_action(&ui, INPUT_DOWN, 9, 0) == PLUGIN_NAV_NONE);
    ui.modal = UI_MODAL_NONE;

    CHECK(plugin_nav_commit_mode(PLUGIN_COMMAND_SAVE) ==
          PLUGIN_COMMIT_SAVE);
    CHECK(plugin_nav_commit_mode(PLUGIN_COMMAND_CONFIRM) ==
          PLUGIN_COMMIT_REBOOT);
    CHECK(plugin_nav_commit_mode(PLUGIN_COMMAND_DISCARD) ==
          PLUGIN_COMMIT_NONE);

    memset(&ui, 0, sizeof(ui));
    ui.tab = UI_TAB_HOME;
    plugin_nav_apply_scroll(&ui, 1000.0f, 1.0f, 120, 400);
    CHECK(ui.home_scroll == 120.0f);
    plugin_nav_apply_scroll(&ui, -1000.0f, 1.0f, 120, 400);
    CHECK(ui.home_scroll == 0.0f);

    ui.tab = UI_TAB_PLUGINS;
    plugin_nav_apply_scroll(&ui, 1000.0f, 1.0f, 120, 400);
    CHECK(ui.plugins_scroll == 400.0f);
    plugin_nav_scroll_plugins(&ui, 100.0f, 400);
    CHECK(ui.plugins_scroll == 400.0f);

    plugin_nav_home_step(&ui, 1, 120);
    CHECK(ui.home_scroll == PLUGIN_NAV_HOME_STEP);
    plugin_nav_home_step(&ui, -1, 120);
    CHECK(ui.home_scroll == 0.0f);
}

static void test_flow_dock_idle(void)
{
    UiState ui;

    flow_fixture(&ui);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_IDLE);

    plugin_flow_dock_enter(&ui);
    CHECK(ui.focus == UI_FOCUS_DOCK);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_RESTORE_ORIGINAL);

    plugin_flow_dock_move(&ui, 1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_RESTORE_SESSION);
    plugin_flow_dock_move(&ui, 1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_RESTORE_SESSION); /* clamp */

    plugin_flow_dock_move(&ui, -1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_RESTORE_ORIGINAL);
    plugin_flow_dock_move(&ui, -1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_RESTORE_ORIGINAL); /* clamp */

    plugin_flow_focus_list(&ui);
    CHECK(ui.focus == UI_FOCUS_LIST);

    /* Triangle toggles in and out. */
    plugin_flow_dock_toggle(&ui);
    CHECK(ui.focus == UI_FOCUS_DOCK);
    plugin_flow_dock_toggle(&ui);
    CHECK(ui.focus == UI_FOCUS_LIST);
}

/* With no snapshot there is no dock action, so focus must stay on the list. */
static void test_flow_dock_no_action(void)
{
    UiState ui;

    flow_fixture(&ui);
    g_editor.original_available = 0;
    g_editor.session_available = 0;

    CHECK(plugin_flow_state() == PLUGIN_FLOW_IDLE);
    plugin_flow_dock_enter(&ui);
    CHECK(ui.focus == UI_FOCUS_LIST);
    plugin_flow_dock_toggle(&ui);
    CHECK(ui.focus == UI_FOCUS_LIST);

    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 1;
    plugin_flow_dock_reset(&ui);
    CHECK(ui.focus == UI_FOCUS_LIST);

    /* With only the session snapshot, entry skips the missing first action. */
    flow_fixture(&ui);
    g_editor.original_available = 0;
    g_editor.session_available = 1;

    plugin_flow_dock_enter(&ui);
    CHECK(ui.focus == UI_FOCUS_DOCK);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_RESTORE_SESSION);
    plugin_flow_dock_move(&ui, -1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_RESTORE_SESSION); /* clamp */
}

/*
 * The battery gate makes CONFIRM unavailable. Left/Right must skip it instead
 * of highlighting a chip the user cannot activate.
 */
static void test_flow_dock_skips_unavailable(void)
{
    UiState ui;
    char err[96];
    int row;

    flow_fixture(&ui);
    fake_set_power(10, 0);
    metrics_sample_slow();

    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_DRAFT);
    CHECK(plugin_flow_action_available(PLUGIN_FLOW_DRAFT,
                                       PLUGIN_DOCK_CONFIRM) == 0);

    plugin_flow_dock_enter(&ui);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_DISCARD);

    plugin_flow_dock_move(&ui, 1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_SAVE);

    plugin_flow_dock_move(&ui, 1); /* CONFIRM is skipped */
    CHECK(dock_action(&ui) == PLUGIN_DOCK_RESTORE_ORIGINAL);

    plugin_flow_dock_move(&ui, 1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_RESTORE_SESSION);
    plugin_flow_dock_move(&ui, 1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_RESTORE_SESSION); /* clamp */

    plugin_flow_dock_move(&ui, -1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_RESTORE_ORIGINAL);
    plugin_flow_dock_move(&ui, -1); /* CONFIRM is skipped backwards too */
    CHECK(dock_action(&ui) == PLUGIN_DOCK_SAVE);
    plugin_flow_dock_move(&ui, -1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_DISCARD);
    plugin_flow_dock_move(&ui, -1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_DISCARD); /* clamp */

    /* A state change resets the cursor to the new safe action. */
    ui.dock_choice = 4;
    plugin_flow_dock_reset(&ui);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_DISCARD);
}

/* SAVED, PREVIEW, and LAST WRITE each reset to their own safe action. */
static void test_flow_dock_states(void)
{
    UiState ui;
    char err[160];
    int row;

    flow_fixture(&ui);
    row = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_SAVED);

    plugin_flow_dock_enter(&ui);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_UNDO);
    plugin_flow_dock_move(&ui, 1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_REBOOT);
    plugin_flow_dock_move(&ui, 1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_REBOOT); /* clamp */

    /*
     * A failed commit keeps the action that failed focused. Only an
     * out-of-range cursor moves back to the safe action.
     */
    ui.dock_choice = 1;
    plugin_flow_dock_keep(&ui);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_REBOOT);
    ui.dock_choice = 9;
    plugin_flow_dock_keep(&ui);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_UNDO);
    plugin_flow_focus_list(&ui);
    ui.dock_choice = 1;
    plugin_flow_dock_keep(&ui);
    CHECK(ui.focus == UI_FOCUS_LIST);

    plugin_flow_dock_enter(&ui);
    ui.dock_choice = 1;
    plugin_flow_dock_reset(&ui);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_UNDO);

    /* Entering the preview changes the state and the safe action. */
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_UNDO, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_PREVIEW);
    CHECK(plugin_flow_action_available(PLUGIN_FLOW_PREVIEW,
                                       PLUGIN_DOCK_CONFIRM) == 1);

    ui.dock_choice = 2;
    plugin_flow_dock_reset(&ui);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_CANCEL);

    plugin_flow_dock_move(&ui, 1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_SAVE);
    plugin_flow_dock_move(&ui, 1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_CONFIRM);
    plugin_flow_dock_move(&ui, 1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_CONFIRM); /* clamp */

    /* Cancel returns to SAVED; a stale cursor resets to UNDO. */
    ui.dock_choice = 2;
    editor_cancel_preview();
    CHECK(plugin_flow_state() == PLUGIN_FLOW_SAVED);
    plugin_flow_dock_reset(&ui);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_UNDO);

    /* After a relaunch the pending difference is gone, but `.undo` remains. */
    editor_scan();
    CHECK(plugin_flow_state() == PLUGIN_FLOW_LAST_WRITE);
    plugin_flow_dock_enter(&ui);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_UNDO);
    plugin_flow_dock_move(&ui, 1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_UNDO); /* only action */
}

/*
 * KTD8 in one place: SAVED keeps the undo while `.undo` differs, SAVED keeps
 * only the reboot once it does not, LAST WRITE is the relaunch truth, and IDLE
 * is neither. A second batch replaces the previous `.undo`.
 */
static void test_flow_saved_truth_table(void)
{
    UiState ui;
    char err[160];
    int kubridge;
    int shellbat;

    flow_fixture(&ui);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_IDLE);
    CHECK(plugin_flow_action_count(PLUGIN_FLOW_IDLE) == 2);

    kubridge = editor_row_for_path("ur0:tai/kubridge.skprx");
    shellbat = editor_row_for_path("ur0:tai/shellbat.suprx");

    CHECK(editor_stage_toggle(kubridge, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_SAVED);
    CHECK(plugin_flow_action_count(PLUGIN_FLOW_SAVED) == 2);
    CHECK(plugin_flow_action_at(PLUGIN_FLOW_SAVED, 0) == PLUGIN_DOCK_UNDO);
    CHECK(plugin_flow_action_at(PLUGIN_FLOW_SAVED, 1) == PLUGIN_DOCK_REBOOT);
    CHECK(plugin_flow_primary_action(PLUGIN_FLOW_SAVED) == PLUGIN_DOCK_REBOOT);

    /* Save B replaces the undo, which now holds the post-A file. */
    CHECK(editor_stage_toggle(shellbat, 1, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_undo_available() == 1);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_SAVED);
    CHECK(plugin_flow_action_count(PLUGIN_FLOW_SAVED) == 2);

    /* Undo B restores A, which still waits for the next boot. */
    CHECK(preview_commit(EDITOR_SNAPSHOT_UNDO, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_undo_available() == 0);
    CHECK(g_editor.pending_count == 1);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_SAVED);
    CHECK(plugin_flow_action_count(PLUGIN_FLOW_SAVED) == 1);
    CHECK(plugin_flow_action_at(PLUGIN_FLOW_SAVED, 0) == PLUGIN_DOCK_REBOOT);
    CHECK(plugin_flow_primary_action(PLUGIN_FLOW_SAVED) == PLUGIN_DOCK_REBOOT);

    /* A relaunch makes the live file the launch set; nothing is pending. */
    editor_scan();
    CHECK(plugin_flow_state() == PLUGIN_FLOW_IDLE);

    /* One save, one relaunch, no pending boot: the undo is the last write. */
    flow_fixture(&ui);
    kubridge = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(editor_stage_toggle(kubridge, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    editor_scan();
    CHECK(plugin_flow_state() == PLUGIN_FLOW_LAST_WRITE);
    CHECK(plugin_flow_action_count(PLUGIN_FLOW_LAST_WRITE) == 1);
    CHECK(plugin_flow_action_at(PLUGIN_FLOW_LAST_WRITE, 0) ==
          PLUGIN_DOCK_UNDO);
    CHECK(plugin_flow_primary_action(PLUGIN_FLOW_LAST_WRITE) ==
          PLUGIN_DOCK_UNDO);

    /*
     * An in-session undo of the only save puts the live file back to the
     * launch bytes and removes `.undo`: there is nothing left to reboot for.
     */
    flow_fixture(&ui);
    kubridge = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(editor_stage_toggle(kubridge, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_SAVED);
    CHECK(preview_commit(EDITOR_SNAPSHOT_UNDO, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_undo_available() == 0);
    CHECK(g_editor.pending_count == 0);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_IDLE);
}

/* Discard writes nothing and returns to the saved state when a save waits. */
static void test_flow_discard_states(void)
{
    UiState ui;
    char err[160];
    int kubridge;
    int shellbat;

    flow_fixture(&ui);
    kubridge = editor_row_for_path("ur0:tai/kubridge.skprx");
    shellbat = editor_row_for_path("ur0:tai/shellbat.suprx");

    CHECK(editor_stage_toggle(shellbat, 1, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_DRAFT);

    int writes = fake_write_count();

    editor_discard_draft();

    CHECK(plugin_flow_state() == PLUGIN_FLOW_IDLE);
    CHECK(g_editor.draft_dirty == 0);
    CHECK(fake_write_count() == writes);

    CHECK(editor_stage_toggle(kubridge, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_SAVED);

    CHECK(editor_stage_toggle(shellbat, 1, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_DRAFT);

    writes = fake_write_count();
    editor_discard_draft();

    CHECK(plugin_flow_state() == PLUGIN_FLOW_SAVED);
    CHECK(g_editor.draft_dirty == 0);
    CHECK(g_editor.pending_count == 1);
    CHECK(fake_write_count() == writes);
}

/*
 * The battery gate protects the reboot, not the write: CONFIRM is unavailable,
 * SAVE is primary, and the same gate covers REBOOT NOW in the saved state.
 */
static void test_flow_battery_gate(void)
{
    UiState ui;
    char err[160];
    char warning[192];
    int row;

    flow_fixture(&ui);
    fake_set_power(10, 0);
    metrics_sample_slow();
    CHECK(plugin_flow_battery_blocked() == 1);

    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_DRAFT);
    CHECK(plugin_flow_action_available(PLUGIN_FLOW_DRAFT,
                                       PLUGIN_DOCK_CONFIRM) == 0);
    CHECK(plugin_flow_action_available(PLUGIN_FLOW_DRAFT,
                                       PLUGIN_DOCK_SAVE) == 1);
    CHECK(plugin_flow_primary_action(PLUGIN_FLOW_DRAFT) == PLUGIN_DOCK_SAVE);

    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 1;
    CHECK(plugin_flow_focused_command(&ui) == PLUGIN_COMMAND_SAVE);
    plugin_flow_dock_warning(&ui, warning, (int)sizeof(warning));
    CHECK(strcmp(warning, "REBOOT BLOCKED - CHARGE TO 20%") == 0);

    /* A stale cursor on the gated confirm is not permission to commit. */
    ui.dock_choice = 2;
    CHECK(plugin_flow_focused_command(&ui) == PLUGIN_COMMAND_NONE);

    /* The frame path revalidates it instead of leaving Cross inert. */
    plugin_flow_revalidate_focus(&ui);
    CHECK(plugin_flow_focused_command(&ui) != PLUGIN_COMMAND_NONE);

    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(editor_undo_available() == 1);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_SAVED);
    CHECK(plugin_flow_action_available(PLUGIN_FLOW_SAVED,
                                       PLUGIN_DOCK_REBOOT) == 0);

    plugin_flow_dock_enter(&ui);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_UNDO);
    plugin_flow_dock_move(&ui, 1);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_UNDO); /* REBOOT is skipped */
    plugin_flow_dock_warning(&ui, warning, (int)sizeof(warning));
    CHECK(strcmp(warning, "REBOOT BLOCKED - CHARGE TO 20%") == 0);

    /* A restore preview uses the same gate and the same primary fallback. */
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_PREVIEW);
    CHECK(plugin_flow_action_available(PLUGIN_FLOW_PREVIEW,
                                       PLUGIN_DOCK_CONFIRM) == 0);
    CHECK(plugin_flow_primary_action(PLUGIN_FLOW_PREVIEW) ==
          PLUGIN_DOCK_SAVE);
    editor_cancel_preview();
    CHECK(plugin_flow_state() == PLUGIN_FLOW_SAVED);

    /* Charging clears the gate even below the threshold. */
    fake_set_power(10, 1);
    metrics_sample_slow();
    CHECK(plugin_flow_battery_blocked() == 0);
    CHECK(plugin_flow_action_available(PLUGIN_FLOW_SAVED,
                                       PLUGIN_DOCK_REBOOT) == 1);

    /* An invalid read blocks it with its own reason. */
    fake_set_power_absent();
    metrics_sample_slow();
    CHECK(plugin_flow_battery_blocked() == 1);
    plugin_flow_dock_warning(&ui, warning, (int)sizeof(warning));
    CHECK(strcmp(warning,
                 "BATTERY LEVEL UNKNOWN - CHARGE BEFORE REBOOTING") == 0);
}

/*
 * A no-change preview and a locked-path removal cannot be saved or confirmed.
 */
static void test_flow_preview_refusals(void)
{
    UiState ui;
    char err[160];
    char warning[192];

    flow_fixture(&ui);
    fake_add_file("ur0:tai/config.txt.session", SAMPLE);
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_PREVIEW);
    CHECK(editor_preview_no_change() == 1);
    CHECK(plugin_flow_action_available(PLUGIN_FLOW_PREVIEW,
                                       PLUGIN_DOCK_CONFIRM) == 0);
    CHECK(plugin_flow_action_available(PLUGIN_FLOW_PREVIEW,
                                       PLUGIN_DOCK_SAVE) == 0);
    CHECK(plugin_flow_primary_action(PLUGIN_FLOW_PREVIEW) ==
          PLUGIN_DOCK_NONE);

    plugin_flow_dock_enter(&ui);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_CANCEL);
    CHECK(plugin_flow_focused_command(&ui) == PLUGIN_COMMAND_CANCEL);
    plugin_flow_dock_warning(&ui, warning, (int)sizeof(warning));
    CHECK(strcmp(warning, "NO CHANGES - NOTHING TO RESTORE") == 0);

    editor_cancel_preview();
    CHECK(plugin_flow_state() == PLUGIN_FLOW_IDLE);

    fake_add_file("ur0:tai/config.txt.session", SAMPLE_NO_HENKAKU);
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_SESSION, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_PREVIEW);
    CHECK(editor_preview_locked_removal() == 1);
    CHECK(plugin_flow_action_available(PLUGIN_FLOW_PREVIEW,
                                       PLUGIN_DOCK_CONFIRM) == 0);
    CHECK(plugin_flow_action_available(PLUGIN_FLOW_PREVIEW,
                                       PLUGIN_DOCK_SAVE) == 0);
    CHECK(plugin_flow_primary_action(PLUGIN_FLOW_PREVIEW) ==
          PLUGIN_DOCK_NONE);

    plugin_flow_dock_enter(&ui);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_CANCEL);
    plugin_flow_dock_warning(&ui, warning, (int)sizeof(warning));
    CHECK(strstr(warning, "RESTORE DROPS") != NULL);
    CHECK(strstr(warning, "henkaku.suprx") != NULL);

    editor_cancel_preview();
    CHECK(plugin_flow_state() == PLUGIN_FLOW_IDLE);
}

/* The focused action maps to the command app.c will run, with no SDK call. */
static void test_flow_focused_commands(void)
{
    UiState ui;
    char err[160];
    int row;

    flow_fixture(&ui);
    CHECK(plugin_flow_focused_command(&ui) == PLUGIN_COMMAND_NONE);

    plugin_flow_dock_enter(&ui);
    CHECK(plugin_flow_focused_command(&ui) ==
          PLUGIN_COMMAND_PREVIEW_ORIGINAL);
    plugin_flow_dock_move(&ui, 1);
    CHECK(plugin_flow_focused_command(&ui) ==
          PLUGIN_COMMAND_PREVIEW_SESSION);

    plugin_flow_focus_list(&ui);
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(editor_stage_toggle(row, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    plugin_flow_dock_reset(&ui);
    plugin_flow_dock_enter(&ui);
    CHECK(dock_action(&ui) == PLUGIN_DOCK_DISCARD);
    CHECK(plugin_flow_focused_command(&ui) == PLUGIN_COMMAND_DISCARD);
    plugin_flow_dock_move(&ui, 1);
    CHECK(plugin_flow_focused_command(&ui) == PLUGIN_COMMAND_SAVE);
    plugin_flow_dock_move(&ui, 1);
    CHECK(plugin_flow_focused_command(&ui) == PLUGIN_COMMAND_CONFIRM);

    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    plugin_flow_dock_reset(&ui);
    CHECK(plugin_flow_state() == PLUGIN_FLOW_SAVED);
    plugin_flow_dock_enter(&ui);
    CHECK(plugin_flow_focused_command(&ui) == PLUGIN_COMMAND_PREVIEW_UNDO);
    plugin_flow_dock_move(&ui, 1);
    CHECK(plugin_flow_focused_command(&ui) == PLUGIN_COMMAND_REBOOT);

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_UNDO, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    plugin_flow_dock_reset(&ui);
    CHECK(plugin_flow_focused_command(&ui) == PLUGIN_COMMAND_CANCEL);
    plugin_flow_dock_move(&ui, 1);
    CHECK(plugin_flow_focused_command(&ui) == PLUGIN_COMMAND_SAVE);
    plugin_flow_dock_move(&ui, 1);
    CHECK(plugin_flow_focused_command(&ui) == PLUGIN_COMMAND_CONFIRM);

    editor_cancel_preview();
    plugin_flow_dock_reset(&ui);

    /* A stale cursor is not permission: the action must still be available. */
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 2;
    CHECK(plugin_flow_focused_command(&ui) == PLUGIN_COMMAND_NONE);
}

/*
 * The inline line names the batch risk and the save-only warning, and an app
 * error outranks both.
 */
static void test_flow_dock_warnings(void)
{
    UiState ui;
    char err[160];
    char warning[192];
    int kernel;
    int shell;

    flow_fixture(&ui);
    kernel = editor_row_for_path("ur0:tai/kubridge.skprx");

    CHECK(editor_stage_toggle(kernel, 0, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 0;
    plugin_flow_dock_warning(&ui, warning, (int)sizeof(warning));
    CHECK(strstr(warning, "HOLD L") != NULL);

    /* The editor's specific storage copy wins over the generic kernel one. */
    CHECK(editor_stage_toggle(editor_row_for_path("ur0:tai/storagemgr.skprx"),
                              0, err, (int)sizeof(err)) == EDITOR_WRITE_OK);
    plugin_flow_dock_warning(&ui, warning, (int)sizeof(warning));
    CHECK(strstr(warning, "UX0 MAY NOT MOUNT") != NULL);

    /* The save-only warning belongs to the focused SAVE chip. */
    flow_fixture(&ui);
    shell = editor_row_for_path("ur0:tai/shellbat.suprx");
    CHECK(editor_stage_toggle(shell, 1, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    ui.focus = UI_FOCUS_DOCK;
    ui.dock_choice = 0;
    plugin_flow_dock_warning(&ui, warning, (int)sizeof(warning));
    CHECK(warning[0] == '\0');
    ui.dock_choice = 1;
    plugin_flow_dock_warning(&ui, warning, (int)sizeof(warning));
    CHECK(strcmp(warning, plugin_flow_save_only_text()) == 0);

    /* A transient app message outranks every derived line. */
    snprintf(ui.dock_message, sizeof(ui.dock_message),
             "CONFIG CHANGED ON DISK. RESTART COCKPIT.");
    plugin_flow_dock_warning(&ui, warning, (int)sizeof(warning));
    CHECK(strcmp(warning, "CONFIG CHANGED ON DISK. RESTART COCKPIT.") == 0);
}

static void test_input_scroll(void)
{
    InputState s;

    input_reset(&s);
    (void)input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.10f,
                       INPUT_CONTEXT_APP, 0.016f);
    CHECK(s.scroll_y == 0.0f);

    (void)input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       INPUT_STICK_DEAD_ZONE, INPUT_CONTEXT_APP, 0.016f);
    CHECK(s.scroll_y == 0.0f);

    (void)input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1.0f,
                       INPUT_CONTEXT_APP, 0.016f);
    CHECK(float_near(s.scroll_y, INPUT_SCROLL_SPEED));

    (void)input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1.0f,
                       INPUT_CONTEXT_APP, 0.016f);
    CHECK(float_near(s.scroll_y, -INPUT_SCROLL_SPEED));

    /* The curve is below linear between the dead zone and the rim. */
    (void)input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.5f,
                       INPUT_CONTEXT_APP, 0.016f);
    CHECK(s.scroll_y > 0.0f);
    CHECK(s.scroll_y < INPUT_SCROLL_SPEED * 0.5f);

    /* The error interrupt ignores the stick and the D-pad (R48). */
    (void)input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1.0f,
                       INPUT_CONTEXT_MODAL, 0.016f);
    CHECK(s.scroll_y == 0.0f);
    CHECK(input_update(&s, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_MODAL, 0.016f) == INPUT_NONE);

    /* Both sticks: the stronger deflection wins, with no addition. */
    CHECK(float_near(input_stronger_stick(0.4f, -0.9f), -0.9f));
    CHECK(float_near(input_stronger_stick(0.7f, 0.2f), 0.7f));
    CHECK(float_near(input_stronger_stick(-0.3f, 0.3f), -0.3f));
}

static void test_input_edges(void)
{
    InputState s;
    input_reset(&s);

    CHECK(input_update(&s, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_ACCEPT);
    CHECK(input_update(&s, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_NONE);

    /* L/R shoulders stay tabs; D-pad Left/Right are their own events. */
    input_reset(&s);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_TAB_LEFT);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_TAB_RIGHT);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_NONE);

    input_reset(&s);
    CHECK(input_update(&s, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_LEFT);
    CHECK(input_update(&s, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_RIGHT);

    input_reset(&s);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_OPTIONS);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_NONE);

    input_reset(&s);
    CHECK(input_update(&s, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_DOWN);
    CHECK(input_update(&s, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.500f) == INPUT_DOWN);
    CHECK(input_update(&s, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.090f) == INPUT_DOWN);

    input_reset(&s);
    CHECK(input_update(&s, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_APP, 0.016f) == INPUT_ACCEPT);
    CHECK(input_update(&s, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_MODAL, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_MODAL, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_MODAL, 0.016f) == INPUT_ACCEPT);

    input_reset(&s);
    CHECK(input_update(&s, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_MODAL, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_MODAL, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_MODAL, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_MODAL, 0.016f) == INPUT_NONE);

    /* The only modal is an acknowledgement: Left/Right do nothing. */
    input_reset(&s);
    CHECK(input_update(&s, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_MODAL, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_MODAL, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0.0f,
                       INPUT_CONTEXT_MODAL, 0.016f) == INPUT_NONE);
}


/* A private system batch inserts under *main, before the next header. */

static void test_orphan_old_is_not_deleted(void)
{
    fake_reset();
    fake_remove_file("ur0:tai/config.txt");
    fake_add_file("ur0:tai/config.txt.old", SAMPLE);
    fake_fail_recover(1);

    CHECK(platform_write_config("ur0:tai/config.txt", "new", 3) != 0);
    CHECK(strcmp(platform_write_error(), "RECOVERY") == 0);
    CHECK(platform_file_exists("ur0:tai/config.txt.old"));
    CHECK(text_is("ur0:tai/config.txt.old", SAMPLE));

    fake_fail_recover(0);
}

static void test_missing_file_is_not_active(void)
{
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*KERNEL\n"
                  "ur0:tai/kubridge.skprx\n"
                  "ur0:tai/absent.skprx\n");
    fake_remove_file("ur0:tai/absent.skprx");

    editor_scan();
    plugins_scan();

    CHECK(editor_row_for_path("ur0:tai/absent.skprx") >= 0);
    CHECK(g_editor.rows[editor_row_for_path("ur0:tai/absent.skprx")].status ==
          EDITOR_MISSING);
    CHECK(g_editor.active_total == 1);
    CHECK(g_editor.disabled_total == 1);
    CHECK(g_editor.loaded_total == 1);
    CHECK(plugins_count() == 1);
}

static void test_preview_sees_section_move(void)
{
    char err[96];

    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*KERNEL\n"
                  "ur0:tai/kubridge.skprx\n");
    fake_add_file("ur0:tai/config.txt.original",
                  "*main\n"
                  "ur0:tai/kubridge.skprx\n");

    editor_scan();
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(row >= 0);

    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_ORIGINAL, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(g_editor.preview_section_change == 1);
    CHECK(g_editor.preview_no_change == 0);
    CHECK(editor_preview_diff(row) == EDITOR_DIFF_MOVE);
    CHECK(strcmp(editor_diff_text(EDITOR_DIFF_MOVE), "WILL MOVE") == 0);
}

static void test_preview_warns_on_kernel_removal(void)
{
    char err[96];

    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*KERNEL\n"
                  "ur0:tai/kubridge.skprx\n"
                  "*main\n"
                  "ur0:tai/henkaku.suprx\n");
    fake_add_file("ur0:tai/config.txt.original",
                  "*main\n"
                  "ur0:tai/henkaku.suprx\n");

    editor_scan();
    CHECK(editor_preview_restore(EDITOR_SNAPSHOT_ORIGINAL, err,
                                 (int)sizeof(err)) == EDITOR_WRITE_OK);
    CHECK(g_editor.preview_removed_count == 1);
    const char *warning = editor_batch_warning();
    CHECK(warning != NULL);
    if (warning != NULL)
        CHECK(strstr(warning, "KERNEL") != NULL);
}

static void test_orphan_scan_failure_is_visible(void)
{
    fake_reset();
    fake_fail_list_dir(1);

    editor_scan();
    CHECK(g_editor.orphans_unverified == 1);

    fake_fail_list_dir(0);
}

static void test_no_live_original_recovery(void)
{
    char err[96];

    fake_reset();
    fake_remove_file("ur0:tai/config.txt");
    fake_add_file("ur0:tai/config.txt.original", SAMPLE);
    editor_scan();

    CHECK(g_editor.source[0] != '\0');
    CHECK(strcmp(g_editor.source, "ur0:tai/config.txt") == 0);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_ORIGINAL) == 1);
    CHECK(editor_can_restore() == 1);
    CHECK(preview_commit(EDITOR_SNAPSHOT_ORIGINAL, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
}

static void test_no_live_session_recovery(void)
{
    char err[96];

    fake_reset();
    fake_remove_file("ur0:tai/config.txt");
    fake_add_file("ur0:tai/config.txt.session", SAMPLE);
    editor_scan();

    CHECK(strcmp(g_editor.source, "ur0:tai/config.txt") == 0);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_SESSION) == 1);
    CHECK(preview_commit(EDITOR_SNAPSHOT_SESSION, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
}

static void test_oversized_undo_repairs(void)
{
    static char big[40000];
    char err[96];

    fake_reset();
    memset(big, '#', sizeof(big) - 2);
    big[sizeof(big) - 2] = '\n';
    big[sizeof(big) - 1] = '\0';
    fake_add_file("ur0:tai/config.txt", big);
    fake_add_file("ur0:tai/config.txt.undo", SAMPLE);

    editor_scan();
    CHECK(g_editor.too_large == 1);
    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_UNDO) == 1);

    CHECK(preview_commit(EDITOR_SNAPSHOT_UNDO, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));
    CHECK(!platform_file_exists("ur0:tai/config.txt.undo"));
}

static void test_comment_only_undo_commits(void)
{
    static const char UNDO_TEXT[] = "# only a comment changed\n" SAMPLE;
    char err[96];

    fake_reset();
    fake_add_file("ur0:tai/config.txt.undo", UNDO_TEXT);
    editor_scan();

    CHECK(editor_snapshot_available(EDITOR_SNAPSHOT_UNDO) == 1);
    CHECK(preview_commit(EDITOR_SNAPSHOT_UNDO, err, (int)sizeof(err)) ==
          EDITOR_WRITE_OK);
    CHECK(text_is("ur0:tai/config.txt", UNDO_TEXT));
    CHECK(!platform_file_exists("ur0:tai/config.txt.undo"));
}

static void test_recover_failure_surfaces(void)
{
    fake_reset();
    fake_remove_file("ur0:tai/config.txt");
    fake_add_file("ur0:tai/config.txt.old", SAMPLE);
    fake_fail_recover(1);

    editor_scan();
    CHECK(g_editor.recover_failed == 1);
    CHECK(strcmp(editor_readonly_reason(), "RECOVERY FAILED") == 0);

    fake_fail_recover(0);
}

static void test_undo_snapshot_failure_preserves_previous(void)
{
    char err[96];
    char buf[64];

    fake_reset();
    fake_add_file("ur0:tai/config.txt.undo", "previous undo\n");
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(row >= 0);
    CHECK(editor_stage_toggle(row, EDITOR_ACTION_DISABLE, err,
                              (int)sizeof(err)) == EDITOR_WRITE_OK);

    fake_fail_rename_path("ur0:tai/config.txt.undo",
                          FAKE_RENAME_FINAL | FAKE_RENAME_RESTORE);
    CHECK(editor_commit_draft(err, (int)sizeof(err)) == EDITOR_WRITE_FAILED);
    CHECK(g_editor.undo_available == 1);
    CHECK(platform_file_exists("ur0:tai/config.txt.undo"));
    CHECK(read_file("ur0:tai/config.txt.undo", buf, (int)sizeof(buf)) > 0);
    CHECK(strcmp(buf, "previous undo\n") == 0);
    CHECK(text_is("ur0:tai/config.txt", SAMPLE));

    fake_fail_rename_path("ur0:tai/config.txt.undo", FAKE_RENAME_NONE);
}

int main(void)
{
    test_scan_sample();
    test_orphan_old_is_not_deleted();
    test_orphan_scan_failure_is_visible();
    test_missing_file_is_not_active();
    test_preview_sees_section_move();
    test_preview_warns_on_kernel_removal();
    test_no_live_original_recovery();
    test_no_live_session_recovery();
    test_oversized_undo_repairs();
    test_comment_only_undo_commits();
    test_recover_failure_surfaces();
    test_undo_snapshot_failure_preserves_previous();
    test_toggle_round_trip();
    test_stage_no_write();
    test_stage_batch();
    test_commit_draft();
    test_commit_stale_refused();
    test_empty_draft();
    test_launch_partial();
    test_restore_layout_keeps_launch();
    test_stage_refusals();
    test_stage_two_sections();
    test_hash_exactness();
    test_backups();
    test_write_failures();
    test_safety_refusals();
    test_statuses();
    test_bom_and_invalid();
    test_case_and_boot();
    test_loaded_vs_next_boot();
    test_locked_missing_agrees();
    test_interrupted_swap_recovery();
    test_undo_snapshot();
    test_scan_reads_undo_only();
    test_preview_original();
    test_preview_session_remove_and_add();
    test_preview_draft_diff();
    test_preview_restore_commit();
    test_preview_restore_failure_and_stale();
    test_preview_locked_removal();
    test_preview_locked_disabled();
    test_preview_locked_halt();
    test_restore_removal_pending();
    test_preview_no_change();
    test_preview_layout_launch();
    test_preview_undo_commit();
    test_preview_undo_failure();
    test_preview_undo_no_live();
    test_preview_undo_no_live_stale();
    test_preview_commit_added_path();
    test_preview_snapshot_unavailable();
    test_preview_availability_cached();
    test_ux0_unreadable_no_fallback();
    test_restore_unreadable_refused();
    test_restore_stage_fake_model();
    test_commit_preserves_previous_undo();
    test_undo_remove_failure();
    test_count_parity();
    test_orphans();
    test_ux0_precedence_and_empty();
    test_flow_dock_idle();
    test_flow_dock_no_action();
    test_flow_dock_skips_unavailable();
    test_flow_dock_states();
    test_flow_saved_truth_table();
    test_flow_discard_states();
    test_flow_battery_gate();
    test_flow_preview_refusals();
    test_flow_focused_commands();
    test_flow_dock_warnings();
    test_plugin_nav();
    test_input_scroll();
    test_input_edges();

    if (g_failures != 0) {
        fprintf(stderr, "%d checks, %d failure(s)\n", g_checks, g_failures);
        return 1;
    }

    printf("%d checks, 0 failure(s)\n", g_checks);
    return 0;
}
