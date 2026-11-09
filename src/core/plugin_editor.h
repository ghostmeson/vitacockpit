#ifndef COCKPIT_CORE_PLUGIN_EDITOR_H
#define COCKPIT_CORE_PLUGIN_EDITOR_H

#include "core/plugins.h"
#include "core/tai_config.h"

/*
 * The editable view of the active taiHEN config (D55, D56, D61-D75).
 *
 * core/plugins.c owns the launch-activated set used by HOME. This module owns
 * the full line model used by PLUGINS: disabled entries, sections, `*!` halts,
 * the working text and its file baseline, staged toggles, the restore preview,
 * the snapshots, and the commit protocol. Keeping them separate is what stops
 * an edit from changing the HOME count before the reboot.
 */

#define EDITOR_TEXT_MAX TAI_CONFIG_MAX
#define EDITOR_LINES_MAX 512
#define EDITOR_ROWS_MAX EDITOR_LINES_MAX
/*
 * Launch facts and the current file baseline can cover disjoint path sets
 * after a restore (the launch set never shrinks by design), so the cache holds
 * both: at most one full launch set plus one full baseline.
 */
#define EDITOR_FILE_ROWS_MAX (EDITOR_ROWS_MAX * 2)
#define EDITOR_REMOVALS_MAX EDITOR_ROWS_MAX
#define EDITOR_ORPHANS_MAX 32
#define EDITOR_BATCH_WARNING_MAX 96

typedef enum {
    EDITOR_LINE_BLANK = 0,
    EDITOR_LINE_COMMENT,
    EDITOR_LINE_HEADER,
    EDITOR_LINE_PATH,
} EditorLineKind;

typedef enum {
    EDITOR_LOADED = 0,
    EDITOR_DISABLED,
    EDITOR_MISSING,
    EDITOR_PARTIAL,
    EDITOR_NOT_REACHED,
    EDITOR_BOOT,
} EditorStatus;

typedef enum {
    EDITOR_ACTION_NONE = -1,
    EDITOR_ACTION_DISABLE = 0,
    EDITOR_ACTION_ENABLE = 1,
} EditorAction;

typedef enum {
    EDITOR_WRITE_OK = 0,
    EDITOR_WRITE_REFUSED,
    EDITOR_WRITE_FAILED,
    EDITOR_WRITE_UNRECOVERED,
} EditorWriteResult;

typedef enum {
    EDITOR_SNAPSHOT_ORIGINAL = 0,
    EDITOR_SNAPSHOT_SESSION,
    EDITOR_SNAPSHOT_UNDO,
} EditorSnapshot;

typedef enum {
    EDITOR_DIFF_UNCHANGED = 0,
    EDITOR_DIFF_ENABLE,
    EDITOR_DIFF_DISABLE,
    EDITOR_DIFF_MOVE,
    EDITOR_DIFF_REMOVE,
} EditorDiff;

typedef struct {
    int start;       /* first byte of the line */
    int len;         /* bytes before the line ending */
    int content;     /* first non-space byte, after a BOM */
    int hash;        /* first '#', or -1 */
    int path_start;  /* first byte of the plugin path */
    int newline;     /* 1 for LF or CR, 2 for CRLF */
    int kind;
    int active;
    int reached;
    PluginSection section;
    char path[PLUGIN_PATH_MAX];
    int row; /* index into rows, or -1 */
} EditorLine;

typedef struct {
    char path[PLUGIN_PATH_MAX];
    char name[PLUGIN_NAME_MAX];
    char desc[PLUGIN_DESC_MAX];
    PluginSection section;
    EditorStatus status;        /* working set: the rocker and the title */
    EditorStatus launch_status; /* launch set: the status badge */
    int active_lines;
    int reached_lines;
    int locked;
    int boot;
    int present;
    int loaded;      /* active in the launch file: what is running now */
    int next_change; /* -1 off at boot, +1 on at boot, 0 no change */
    int draft_change;  /* -1 disabling, +1 enabling, 0 file baseline */
    int preview_before; /* working-set activity before the preview */
    int duplicate_name; /* another row shares this basename */
} EditorRow;

typedef struct {
    /*
     * `text` is the working set the rows describe. `last_text` is the file
     * baseline read at scan and adopted again after a commit, so a draft is a
     * difference between the two and never needs a second file read.
     */
    char text[EDITOR_TEXT_MAX];
    int text_len;

    char last_text[EDITOR_TEXT_MAX];
    int last_len;

    char source[PLUGIN_PATH_MAX];
    char ignored[PLUGIN_PATH_MAX];
    int source_ok;
    int valid;        /* no path before the first section, no bad path */
    int read_only;
    int too_large;
    int invalid;      /* warning only: a path before the first section */
    int unreadable;
    int paths_truncated; /* a path was too long and was skipped */
    int orphans_unverified;
    int truncated;        /* rows were dropped by the row ceiling */
    int rows_truncated;
    int lines_truncated;
    int backup_warning;
    int original_snapshot_present; /* .original exists or was written at scan */
    int session_snapshot_written;  /* .session was written from this scan */
    int original_available; /* cached: .original is readable */
    int session_available;  /* cached: .session is readable */
    int undo_available;     /* cached: readable .undo differs from live */
    int write_unrecovered;  /* the last write failed and could not be undone */
    int recovered;      /* a .old file was moved back into place at scan */
    int recover_failed; /* a .old file exists but could not be moved back */

    EditorLine lines[EDITOR_LINES_MAX];
    int line_count;

    EditorRow rows[EDITOR_ROWS_MAX];
    int row_count;

    int pending_count;
    int loaded_total;     /* rows active in the launch file */
    int active_total;     /* working-set rows with a reached active line */
    int disabled_total;   /* every other row */
    int draft_dirty;      /* working text differs from the file baseline */
    int draft_count;      /* rows whose working activity differs from file */
    int draft_on_total;   /* rows enabled since the file baseline */
    int draft_off_total;  /* rows disabled since the file baseline */
    char batch_warning[EDITOR_BATCH_WARNING_MAX];
    int batch_warning_set; /* batch_warning holds a note for the dock */

    /*
     * The restore preview swaps the working text for the selected snapshot.
     * saved_work is the draft or file state to put back on cancel; removed[]
     * is every pre-preview working path the snapshot does not list. The
     * no-change flag and the protected-drop flag (a BOOT or locked HENKAKU
     * path missing or disabled) are what the dock reads, and are recomputed
     * only when a preview opens or closes.
     */
    int preview_active;
    EditorSnapshot preview_kind;
    int preview_no_change;
    int preview_enable_total;  /* rows the snapshot enables */
    int preview_disable_total; /* rows the snapshot disables */
    int preview_section_change; /* a row moved between load targets */
    int preview_kernel_change;  /* a KERNEL row moved or was removed */
    int preview_discards_draft;
    int preview_locked_removal;
    char preview_locked_path[PLUGIN_PATH_MAX];
    int preview_saved_work_len;
    int preview_saved_dirty;
    int preview_work_count;
    int preview_removed_count;

    char orphans[EDITOR_ORPHANS_MAX][PLUGIN_PATH_MAX];
    int orphan_shadowed[EDITOR_ORPHANS_MAX];
    int orphan_count;
    int orphan_truncated;
} EditorState;

extern EditorState g_editor;

/* Snapshot the config set, resolve the active file, parse it, and build rows. */
void editor_scan(void);

/*
 * The launch snapshot result. `.original` is present when it already existed or
 * was written at this scan; `.session` is written only from this scan's bytes.
 * The launch card reads these instead of inferring from `backup_warning` or the
 * readable-snapshot probes, which answer different questions.
 */
int editor_original_snapshot_present(void);
int editor_session_snapshot_written(void);

/*
 * Stage the next toggle in memory. No platform read, stat, write, or snapshot
 * is performed: the working text is rebuilt from itself, reparsed, and the
 * counts move. `err` carries a short, font-safe message when the row refuses
 * the change.
 */
EditorWriteResult editor_stage_toggle(int row, EditorAction mode, char *err,
                                      int err_cap);

/* Drop the draft and return the working set to the file baseline. No write. */
void editor_discard_draft(void);

/*
 * Write the whole working set once. The live file must still match the scan
 * baseline; `.undo` is snapshotted from that baseline before the write. On
 * success the working text becomes the new baseline and the draft clears. On
 * failure the draft stays so the user can retry.
 */
EditorWriteResult editor_commit_draft(char *err, int err_cap);

int editor_draft_dirty(void);
int editor_draft_count(void);

/* -1 disabling, +1 enabling, 0 the row matches the file baseline. */
int editor_draft_change(int row);

/*
 * Read a snapshot once and show it as the working set without writing. The
 * diff is against the pre-preview working set (the draft when one exists), not
 * against the file. A readable snapshot is available even when it matches the
 * working set; that case reports preview_no_change and refuses a commit.
 *
 * `err` carries a short, font-safe message when the snapshot is missing, too
 * large, or too complex to model.
 */
EditorWriteResult editor_preview_restore(EditorSnapshot kind, char *err,
                                         int err_cap);

/* Put the pre-preview working text back. Never reads, stats, or writes. */
void editor_cancel_preview(void);

/*
 * Write the snapshot previewed by editor_preview_restore once. A restore
 * snapshots the live file to `.undo` before the write; an UNDO preview writes
 * the bytes it already read and never rewrites `.undo` from the live file.
 * The `.undo` is removed only after a verified UNDO write. A stale file, a
 * no-change diff, and a BOOT or locked HENKAKU drop (absent or disabled) are
 * refused.
 */
EditorWriteResult editor_commit_preview(char *err, int err_cap);

/*
 * Cached snapshot facts for the dock and render path. The flags are updated by
 * scan and by a commit, restore, or undo; querying them never touches the card.
 */
int editor_snapshot_available(EditorSnapshot kind);
int editor_undo_available(void);

/* The cached launch activity for a path, or -1 when the path is unknown. */
int editor_launch_active_for_path(const char *path);

int editor_preview_active(void);
int editor_preview_no_change(void);
int editor_preview_discards_draft(void);
int editor_preview_locked_removal(void);

/* The first BOOT or locked HENKAKU path the snapshot would drop (missing or
 * disabled), or NULL. */
const char *editor_preview_locked_path(void);

int editor_preview_removed_count(void);
const char *editor_preview_removed_path(int index);

/* The preview's plugin-level change for a snapshot row, and its label. */
EditorDiff editor_preview_diff(int row);
const char *editor_diff_text(EditorDiff diff);

/*
 * The next action for the row, or EDITOR_ACTION_NONE.
 */
EditorAction editor_next_action(int row);

const char *editor_status_text(EditorStatus status);
const char *editor_section_text(PluginSection section);

/* The basename, or the full path when two rows share a basename. */
const char *editor_row_label(int row);

/* Why the whole page is read-only, or NULL when editing is possible. */
const char *editor_readonly_reason(void);

/*
 * 1 when an ORIGINAL or SESSION snapshot may be restored. A live file the
 * parser could not read or model cannot be backed up before the restore, so
 * the whole-file replace is refused instead of overwriting bytes the app never
 * saw (R5, R43, R56). An unrecovered write must be repaired by undo first.
 * Undo is separate: it repairs a live file that is absent, unreadable, or
 * partially replaced.
 */
int editor_can_restore(void);

/*
 * The specific risk note for toggling the row in `mode`, or NULL when the row
 * carries none. The dock reads the cached batch warning; this is what builds
 * it.
 */
const char *editor_confirm_text(int row, EditorAction mode);

/* The cached batch warning for the dock, or NULL when there is none. */
const char *editor_batch_warning(void);

int editor_row_for_path(const char *path);

#endif
