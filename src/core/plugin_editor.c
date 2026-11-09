#include "core/plugin_editor.h"

#include <stdio.h>
#include <string.h>

#include "core/platform.h"
#include "core/tai_config.h"

EditorState g_editor;

/*
 * Three shared 32 KiB scratch buffers. The editor never needs more than these
 * at once: a scan read, a work buffer (toggle output / live stale check), and a
 * snapshot read (availability probe, preview source, or undo bytes). Keeping
 * them file-scope avoids seven separate statics (~224 KiB of BSS) without
 * putting them on the stack.
 */
static char s_scan[EDITOR_TEXT_MAX];
static char s_work[EDITOR_TEXT_MAX];
static char s_undo[EDITOR_TEXT_MAX];

/*
 * The preview's scratch is private to this module: the header exposes only the
 * counts the dock reads, so a cache reshape cannot reach the panel or tests.
 */
/*
 * One path's cached facts. `active_lines`/`reached_lines` are the file baseline
 * used to tell a draft row from its saved bytes; `launch_listed` says the path
 * was in the file read at scan, and `launch_*` is the set the console started
 * with, kept by path so a reparse with a different line layout cannot move a
 * badge onto the wrong row. A path first seen in a snapshot has launch_listed
 * 0: it was not in the launch file, even though a later restore may adopt it.
 */
typedef struct {
    char path[PLUGIN_PATH_MAX];
    int present;
    int active_lines;
    int reached_lines;
    int launch_listed;
    int launch_active;
    int launch_reached;
    int launch_present;
} EditorFileRow;

typedef struct {
    char path[PLUGIN_PATH_MAX];
    int active_lines;
    PluginSection section;
} EditorWorkRow;

static EditorFileRow s_file_rows[EDITOR_FILE_ROWS_MAX];
static int s_file_row_count;

static char s_preview_saved_work[EDITOR_TEXT_MAX];
static EditorWorkRow s_preview_work[EDITOR_ROWS_MAX];
static char s_preview_removed[EDITOR_REMOVALS_MAX][PLUGIN_PATH_MAX];

static int matches_last_text(const char *buf, int len)
{
    return len == g_editor.last_len &&
           memcmp(buf, g_editor.last_text, (size_t)len) == 0;
}

static int row_for_path(const EditorState *e, const char *path);

static const char *const TAI_DIRS[2] = {TAI_DIR_UR0, TAI_DIR_UX0};

#define KERNEL_BATCH_WARNING                                               \
    "KERNEL PLUGIN IN BATCH. IF BAD, BOOT MAY STOP. HOLD L AT POWER-ON "   \
    "TO SKIP."

static int is_storage_driver(const char *name)
{
    return tai_ieq(name, "storagemgr.skprx") || tai_ieq(name, "usbmc.skprx") ||
           tai_ieq(name, "usbmcm.skprx");
}

static int is_boot_name(const char *name)
{
    return tai_ieq(name, "taihen.skprx") || tai_ieq(name, "henkaku.skprx") ||
           tai_ieq(name, "yamt.skprx");
}

static int is_boot_path(const EditorState *e, const char *path)
{
    (void)e;

    /*
     * boot_config.txt is deliberately not opened. The three modules below are
     * the ones documented as loading before taiHEN reads config.txt, so they
     * stay locked by name; any other boot module is outside this app's scope.
     */
    return is_boot_name(tai_base_name(path));
}

/*
 * A path the editor refuses to switch off: a pre-taiHEN boot module or the
 * locked HENKAKU recovery path. A restore may not drop one by omitting it or
 * by leaving every reached line commented out.
 */
static int is_protected_path(const EditorState *e, const char *path)
{
    return is_boot_path(e, path) ||
           tai_ieq(tai_base_name(path), "henkaku.suprx");
}

static int row_for_path(const EditorState *e, const char *path)
{
    for (int i = 0; i < e->row_count; ++i) {
        if (tai_ieq(e->rows[i].path, path))
            return i;
    }

    return -1;
}

static void rebuild_counts(EditorState *e);
static void refresh_draft(EditorState *e);
static const char *confirm_text_for(const char *name, EditorAction mode);

static EditorFileRow *file_row_for_path(const char *path)
{
    for (int i = 0; i < s_file_row_count; ++i) {
        if (tai_ieq(s_file_rows[i].path, path))
            return &s_file_rows[i];
    }

    return NULL;
}

/*
 * One status rule for both truths. The working set passes its parsed counts;
 * the launch set passes the counts cached in the file-row cache. Presence belongs to
 * the cache too, so neither caller stats the card.
 */
static EditorStatus status_for(int boot, int reached, int active, int present)
{
    if (boot)
        return EDITOR_BOOT;
    if (reached == 0)
        return EDITOR_NOT_REACHED;
    if (!present)
        return EDITOR_MISSING;
    if (active > 0 && active < reached)
        return EDITOR_PARTIAL;
    if (active > 0)
        return EDITOR_LOADED;

    return EDITOR_DISABLED;
}

/*
 * A path first seen in a snapshot was not in the launch file, so its badge is
 * NOT LOADED even though the snapshot proposes to enable it. `launch_listed`
 * is the one fact that separates that from a path the launch file listed after
 * a `*!` halt (which stays NOT REACHED).
 */
static EditorStatus launch_status_for(int boot, int listed, int reached,
                                      int active, int present)
{
    if (!listed)
        return boot ? EDITOR_BOOT : EDITOR_DISABLED;

    return status_for(boot, reached, active, present);
}

static void rebuild_rows(EditorState *e)
{
    int order[EDITOR_ROWS_MAX];

    memset(e->rows, 0, sizeof(e->rows));
    e->row_count = 0;
    e->pending_count = 0;

    for (int i = 0; i < e->line_count; ++i) {
        EditorLine *l = &e->lines[i];

        if (l->kind != EDITOR_LINE_PATH)
            continue;

        int ri = row_for_path(e, l->path);
        if (ri < 0) {
            if (e->row_count >= EDITOR_ROWS_MAX) {
                e->truncated = 1;
                e->rows_truncated = 1;
                continue;
            }

            ri = e->row_count++;
            order[ri] = ri;

            EditorRow *r = &e->rows[ri];
            tai_copy(r->path, sizeof(r->path), l->path);
            tai_copy(r->name, sizeof(r->name), tai_base_name(l->path));
            tai_copy(r->desc, sizeof(r->desc), plugins_desc_for(r->name));
            r->section = l->section;
        }

        EditorRow *r = &e->rows[ri];
        if (l->section < r->section)
            r->section = l->section;

        if (l->reached) {
            ++r->reached_lines;
            if (l->active)
                ++r->active_lines;
        }

        l->row = ri;
    }

    for (int i = 0; i < e->row_count; ++i) {
        EditorRow *r = &e->rows[i];

        r->locked = tai_ieq(r->name, "henkaku.suprx");
        r->boot = is_boot_path(e, r->path);

        /*
         * The launch facts live by path, not by line index: a reparse after a
         * stage, a discard, or a restore with a different layout must not move
         * a badge onto a different row. The first parse of the scan seeds them;
         * every later parse reads them. A path first seen in a snapshot is
         * cached as not listed, with one presence check for that path.
         */
        EditorFileRow *f = file_row_for_path(r->path);

        if (f == NULL) {
            if (s_file_row_count >= EDITOR_FILE_ROWS_MAX) {
                e->truncated = 1;
                e->rows_truncated = 1;
                continue;
            }

            f = &s_file_rows[s_file_row_count++];
            memset(f, 0, sizeof(*f));
            tai_copy(f->path, sizeof(f->path), r->path);
            f->present = platform_file_exists(r->path);
            f->active_lines = r->active_lines;
            f->reached_lines = r->reached_lines;

            /*
             * The first parse of the scan seeds the launch set. A path that
             * first appears in a preview is not part of it: the launch file
             * never listed the path, so it cannot have loaded it. The one
             * presence check above is cached here for the rest of the preview;
             * no later parse stats it again.
             */
            f->launch_listed = e->preview_active ? 0 : 1;
            if (f->launch_listed) {
                f->launch_active = r->active_lines;
                f->launch_reached = r->reached_lines;
                f->launch_present = f->present;
            }
        }

        r->present = f->present;
        r->loaded = f->launch_active > 0 && f->launch_present;
        r->status = status_for(r->boot, r->reached_lines, r->active_lines,
                               r->present);
        r->launch_status = launch_status_for(r->boot, f->launch_listed,
                                             f->launch_reached,
                                             f->launch_active,
                                             f->launch_present);

        /*
         * NEXT ON / NEXT OFF compares the working set to launch, not to the
         * file: after a save the rows keep saying what changes at boot.
         */
        if (r->active_lines != f->launch_active)
            r->next_change = r->active_lines > 0 ? 1 : -1;

        if (r->next_change != 0)
            ++e->pending_count;
    }

    /*
     * A restore can remove a path that was active at launch. It has no row, but
     * the console still runs it until the reboot; count that difference so the
     * dock keeps SAVED and offers REBOOT NOW (R36). A truncated row model
     * cannot prove which paths it failed to list, so fail closed instead.
     */
    if (!e->rows_truncated) {
        for (int i = 0; i < s_file_row_count; ++i) {
            const EditorFileRow *f = &s_file_rows[i];

            if (f->launch_listed && f->launch_active > 0 &&
                row_for_path(e, f->path) < 0)
                ++e->pending_count;
        }
    }

    /*
     * Display order is section-major, so the selection index must follow the
     * same order the panel draws. The sort is stable: rows in one section keep
     * the order their first line appeared in the file. `order` carries the
     * pre-sort row index so the line remap below is O(lines + rows), not a
     * second path search.
     */
    for (int i = 1; i < e->row_count; ++i) {
        EditorRow tmp = e->rows[i];
        int tmp_order = order[i];
        int j = i - 1;

        while (j >= 0 && e->rows[j].section > tmp.section) {
            e->rows[j + 1] = e->rows[j];
            order[j + 1] = order[j];
            --j;
        }
        e->rows[j + 1] = tmp;
        order[j + 1] = tmp_order;
    }

    int new_index[EDITOR_ROWS_MAX];

    for (int i = 0; i < e->row_count; ++i)
        new_index[order[i]] = i;

    for (int i = 0; i < e->line_count; ++i) {
        EditorLine *l = &e->lines[i];

        if (l->kind == EDITOR_LINE_PATH && l->row >= 0)
            l->row = new_index[l->row];
    }

    for (int i = 0; i < e->row_count; ++i) {
        EditorRow *r = &e->rows[i];

        for (int j = 0; j < e->row_count; ++j) {
            if (i != j && tai_ieq(r->name, e->rows[j].name)) {
                r->duplicate_name = 1;
                break;
            }
        }
    }

    rebuild_counts(e);
}

/*
 * Counts describe the working set. `loaded_total` is the launch set and is
 * counted once per row, not once per line: a row with several active lines adds
 * one. ACTIVE counts a row with at least one reached, uncommented line;
 * DISABLED is every other row (R10).
 */
static void rebuild_counts(EditorState *e)
{
    e->loaded_total = 0;
    e->active_total = 0;
    e->disabled_total = 0;

    for (int i = 0; i < e->row_count; ++i) {
        if (e->rows[i].loaded)
            ++e->loaded_total;

        if (e->rows[i].present && e->rows[i].active_lines > 0)
            ++e->active_total;
        else
            ++e->disabled_total;
    }

    /* The launch set keeps paths a restore removed from the current rows. */
    if (!e->rows_truncated) {
        for (int i = 0; i < s_file_row_count; ++i) {
            const EditorFileRow *f = &s_file_rows[i];

            if (f->launch_listed && f->launch_active > 0 &&
                f->launch_present && row_for_path(e, f->path) < 0)
                ++e->loaded_total;
        }
    }
}

/*
 * One note for the whole batch. A specific row warning (storage, licence) wins
 * over the generic kernel rule, which is what `HOLD L` at power-on is for.
 */
static void refresh_batch_warning(EditorState *e, int preview)
{
    int kernel = 0;

    e->batch_warning[0] = '\0';
    e->batch_warning_set = 0;

    for (int i = 0; i < e->row_count; ++i) {
        int change;

        if (preview) {
            int after = e->rows[i].active_lines;
            int before = e->rows[i].preview_before;

            change = after > before ? 1 : (after < before ? -1 : 0);
        } else {
            change = e->rows[i].draft_change;
        }

        if (change == 0)
            continue;

        EditorAction mode = change > 0 ? EDITOR_ACTION_ENABLE
                                       : EDITOR_ACTION_DISABLE;
        const char *note = editor_confirm_text(i, mode);

        if (note != NULL) {
            snprintf(e->batch_warning, sizeof(e->batch_warning), "%s", note);
            e->batch_warning_set = 1;
            return;
        }

        if (e->rows[i].section == PLUGIN_SECTION_KERNEL)
            kernel = 1;
    }

    /*
     * A removed path has no row, so the loop above cannot warn about it. The
     * whole-path removal is stronger than a disable and needs the same note.
     */
    if (preview) {
        for (int i = 0; i < e->preview_removed_count; ++i) {
            const char *note = confirm_text_for(
                tai_base_name(s_preview_removed[i]), EDITOR_ACTION_DISABLE);

            if (note != NULL) {
                snprintf(e->batch_warning, sizeof(e->batch_warning), "%s",
                         note);
                e->batch_warning_set = 1;
                return;
            }
        }
    }

    if (preview && e->preview_kernel_change)
        kernel = 1;

    if (kernel) {
        snprintf(e->batch_warning, sizeof(e->batch_warning), "%s",
                 KERNEL_BATCH_WARNING);
        e->batch_warning_set = 1;
    }
}

/*
 * A draft is a difference from the file baseline, so both flags compare against
 * `last_text` and the file-row cache. A stage and its reverse therefore leave an
 * empty draft without a special case.
 */
static void refresh_draft(EditorState *e)
{
    /*
     * During a preview, `text` is the snapshot, not the draft. The pre-preview
     * draft flags stay as they were; cancel can then put the draft back without
     * a second file read.
     */
    if (e->preview_active)
        return;

    e->draft_dirty = e->text_len != e->last_len ||
                     memcmp(e->text, e->last_text, (size_t)e->text_len) != 0;
    e->draft_count = 0;
    e->draft_on_total = 0;
    e->draft_off_total = 0;

    for (int i = 0; i < e->row_count; ++i) {
        const EditorFileRow *f = file_row_for_path(e->rows[i].path);
        int change = 0;

        if (f != NULL) {
            if (e->rows[i].active_lines > f->active_lines)
                change = 1;
            else if (e->rows[i].active_lines < f->active_lines)
                change = -1;
        }

        e->rows[i].draft_change = change;

        if (change != 0) {
            ++e->draft_count;

            if (change > 0)
                ++e->draft_on_total;
            else
                ++e->draft_off_total;
        }
    }

    refresh_batch_warning(e, 0);
}

/*
 * Drop baseline entries that are neither launch facts nor part of the current
 * rows. A preview may introduce paths (and a later restore may adopt some), so
 * the table has to stay bounded across a session without forgetting what the
 * console launched.
 */
static void prune_file_rows(EditorState *e)
{
    int out = 0;

    for (int i = 0; i < s_file_row_count; ++i) {
        EditorFileRow *f = &s_file_rows[i];

        if (f->launch_listed || row_for_path(e, f->path) >= 0)
            s_file_rows[out++] = *f;
    }

    s_file_row_count = out;
}

/* Adopt the parsed working text as the file baseline without re-reading it. */
static void sync_file_baseline(EditorState *e)
{
    for (int i = 0; i < e->row_count; ++i) {
        EditorFileRow *f = file_row_for_path(e->rows[i].path);

        if (f == NULL) {
            if (s_file_row_count >= EDITOR_FILE_ROWS_MAX)
                continue;

            f = &s_file_rows[s_file_row_count++];
            memset(f, 0, sizeof(*f));
            tai_copy(f->path, sizeof(f->path), e->rows[i].path);
            f->present = e->rows[i].present;
            f->launch_listed = 0;
        }

        f->active_lines = e->rows[i].active_lines;
        f->reached_lines = e->rows[i].reached_lines;
    }

    prune_file_rows(e);
    refresh_draft(e);
}

static void parse_text(EditorState *e, const char *text, int len)
{
    if (len > EDITOR_TEXT_MAX - 1)
        len = EDITOR_TEXT_MAX - 1;

    /* `text` may already be e->text when a commit adopts the working set. */
    if (text != e->text)
        memcpy(e->text, text, (size_t)len);
    e->text[len] = '\0';
    e->text_len = len;
    e->line_count = 0;
    e->valid = 1;
    e->invalid = 0;
    e->lines_truncated = 0;
    e->paths_truncated = 0;
    e->rows_truncated = 0;
    e->truncated = 0;

    int have_section = 0;
    int halted = 0;
    PluginSection section = PLUGIN_SECTION_TITLE;
    int i = 0;

    while (i < len && e->line_count < EDITOR_LINES_MAX) {
        int start = i;

        while (i < len && text[i] != '\n' && text[i] != '\r')
            ++i;

        int end = i;
        int newline = 0;

        if (i < len) {
            if (text[i] == '\r' && i + 1 < len && text[i + 1] == '\n') {
                i += 2;
                newline = 2;
            } else {
                ++i;
                newline = 1;
            }
        }

        int content = start;
        while (content < end && (text[content] == ' ' || text[content] == '\t'))
            ++content;

        /* A UTF-8 BOM belongs to the file, not to the first header. */
        if (start == 0 && content == 0 && tai_has_bom(text, len)) {
            content = TAI_BOM_LEN;
            while (content < end &&
                   (text[content] == ' ' || text[content] == '\t'))
                ++content;
        }

        int content_end = end;
        while (content_end > content &&
               (text[content_end - 1] == ' ' || text[content_end - 1] == '\t'))
            --content_end;

        EditorLine *l = &e->lines[e->line_count];
        memset(l, 0, sizeof(*l));
        l->start = start;
        l->len = end - start;
        l->content = content;
        l->hash = -1;
        l->path_start = content;
        l->newline = newline;
        l->section = section;
        l->row = -1;

        if (content == content_end) {
            l->kind = EDITOR_LINE_BLANK;
        } else if (text[content] == '*') {
            l->kind = EDITOR_LINE_HEADER;
            if (content + 1 < content_end && text[content + 1] == '!') {
                halted = 1;
                have_section = 1;
            } else {
                section = tai_section_from_header(text + content,
                                                  content_end - content);
                have_section = 1;
            }
            l->section = section;
        } else {
            /*
             * A disabled line may carry one or more '#' markers, with spaces
             * between them. The path begins after the whole comment run. This
             * makes `##foo.skprx` and `# # foo.skprx` uncomment cleanly instead
             * of leaving a '#' behind.
             */
            int p = content;
            int hash = -1;
            int hash_count = 0;

            while (p < content_end && text[p] == '#') {
                if (hash < 0)
                    hash = p;
                ++hash_count;
                ++p;
                while (p < content_end &&
                       (text[p] == ' ' || text[p] == '\t'))
                    ++p;
            }

            int path_start = p;
            int active = hash_count == 0;

            if (path_start < content_end &&
                tai_path_valid(text + path_start, content_end - path_start)) {
                if (content_end - path_start >= PLUGIN_PATH_MAX) {
                    /*
                     * One long path must not make the whole file read-only.
                     * Skip the entry and warn, exactly as the VITALS parser does
                     * (D65, D71).
                     */
                    l->kind = EDITOR_LINE_COMMENT;
                    e->paths_truncated = 1;
                } else {
                    l->kind = EDITOR_LINE_PATH;
                    l->active = active;
                    l->reached = have_section && !halted;
                    l->hash = hash;
                    l->path_start = path_start;

                    tai_copy_slice(l->path, sizeof(l->path), text + path_start,
                                   content_end - path_start);
                    if (!have_section)
                        e->valid = 0;
                }
            } else {
                l->kind = EDITOR_LINE_COMMENT;
            }
        }

        ++e->line_count;
    }

    e->invalid = e->valid ? 0 : 1;

    if (i < len) {
        e->truncated = 1;
        e->lines_truncated = 1;
    }

    rebuild_rows(e);
    refresh_draft(e);
}

static void clear_preview_state(EditorState *e)
{
    e->preview_active = 0;
    e->preview_kind = EDITOR_SNAPSHOT_ORIGINAL;
    e->preview_no_change = 0;
    e->preview_enable_total = 0;
    e->preview_disable_total = 0;
    e->preview_discards_draft = 0;
    e->preview_locked_removal = 0;
    e->preview_locked_path[0] = '\0';
    e->preview_section_change = 0;
    e->preview_kernel_change = 0;
    e->preview_work_count = 0;
    e->preview_removed_count = 0;
}

/*
 * Put the pre-preview working text back. The preview may have added baseline
 * entries for paths the snapshot listed; prune drops the ones the restored
 * working set does not use.
 */
static void restore_saved_work(EditorState *e)
{
    int len = e->preview_saved_work_len;
    int dirty = e->preview_saved_dirty;

    clear_preview_state(e);
    parse_text(e, s_preview_saved_work, len);
    e->draft_dirty = dirty;
    prune_file_rows(e);
}

typedef struct {
    EditorState *e;
    const char *dir;
} OrphanCtx;

static void orphan_cb(const char *name, int is_dir, void *user)
{
    OrphanCtx *ctx = (OrphanCtx *)user;
    EditorState *e = ctx->e;

    if (is_dir || !tai_is_plugin_path(name))
        return;
    if (tai_ends_with(name, ".bak") || tai_ends_with(name, ".new") ||
        tai_ends_with(name, ".old") || tai_ends_with(name, ".original") ||
        tai_ends_with(name, ".session") || tai_ends_with(name, ".undo"))
        return;

    if (e->orphan_count >= EDITOR_ORPHANS_MAX) {
        e->orphan_truncated = 1;
        return;
    }

    char full[PLUGIN_PATH_MAX];
    int dirlen = (int)strlen(ctx->dir);
    int namelen = (int)strlen(name);

    if (dirlen + 1 + namelen >= (int)sizeof(full)) {
        e->orphan_truncated = 1;
        return;
    }

    memcpy(full, ctx->dir, (size_t)dirlen);
    full[dirlen] = '/';
    memcpy(full + dirlen + 1, name, (size_t)namelen + 1);

    if (row_for_path(e, full) >= 0)
        return;
    if (is_boot_path(e, full))
        return;

    tai_copy(e->orphans[e->orphan_count], sizeof(e->orphans[0]), full);
    e->orphan_shadowed[e->orphan_count] =
        e->ignored[0] != '\0' &&
        strncmp(ctx->dir, e->ignored, strlen(ctx->dir)) == 0;
    ++e->orphan_count;
}

static void sort_orphans(EditorState *e)
{
    for (int i = 1; i < e->orphan_count; ++i) {
        char path[PLUGIN_PATH_MAX];
        int shadowed = e->orphan_shadowed[i];

        tai_copy(path, sizeof(path), e->orphans[i]);

        int j = i - 1;
        while (j >= 0 && strcmp(e->orphans[j], path) > 0) {
            tai_copy(e->orphans[j + 1], sizeof(e->orphans[0]), e->orphans[j]);
            e->orphan_shadowed[j + 1] = e->orphan_shadowed[j];
            --j;
        }

        tai_copy(e->orphans[j + 1], sizeof(e->orphans[0]), path);
        e->orphan_shadowed[j + 1] = shadowed;
    }
}

/*
 * Probe the three snapshots once, at scan. The dock reads only these flags;
 * nothing on the render path opens a file. `.undo` is available only when it
 * differs from the live file, because an undo that writes the same bytes is
 * not a recovery. With no live baseline any readable `.undo` is useful.
 */
static void refresh_snapshot_availability(void)
{
    g_editor.original_available = 0;
    g_editor.session_available = 0;
    g_editor.undo_available = 0;

    if (g_editor.source[0] == '\0')
        return;

    int n = 0;

    if (platform_read_backup(g_editor.source, PLATFORM_BACKUP_ORIGINAL,
                             s_undo, (int)sizeof(s_undo),
                             &n) == PLATFORM_READ_OK)
        g_editor.original_available = 1;

    if (platform_read_backup(g_editor.source, PLATFORM_BACKUP_SESSION, s_undo,
                             (int)sizeof(s_undo),
                             &n) == PLATFORM_READ_OK)
        g_editor.session_available = 1;

    if (platform_read_backup(g_editor.source, PLATFORM_BACKUP_UNDO, s_undo,
                             (int)sizeof(s_undo), &n) == PLATFORM_READ_OK &&
        (!g_editor.source_ok || !matches_last_text(s_undo, n)))
        g_editor.undo_available = 1;
}

void editor_scan(void)
{
    memset(&g_editor, 0, sizeof(g_editor));
    memset(s_file_rows, 0, sizeof(s_file_rows));
    s_file_row_count = 0;

    int n = 0;

    /*
     * A crash or power loss between the two renames of the swap leaves the
     * config at `.old` and the live name empty. Recover it before deciding the
     * config is missing; otherwise the app would offer to create a new file
     * while the user's bytes sit one name away (D71).
     */
    int rec_ux0 = platform_recover_swap(TAI_CONFIG_UX0);
    int rec_ur0 = platform_recover_swap(TAI_CONFIG_UR0);

    if (rec_ux0 > 0 || rec_ur0 > 0)
        g_editor.recovered = 1;
    if (rec_ux0 < 0 || rec_ur0 < 0)
        g_editor.recover_failed = 1;

    PlatformReadStatus ux0 = platform_read_config(
        TAI_CONFIG_UX0, s_scan, (int)sizeof(s_scan), &n);
    PlatformReadStatus ur0 = PLATFORM_READ_ABSENT;

    if (ux0 == PLATFORM_READ_ABSENT) {
        ur0 = platform_read_config(TAI_CONFIG_UR0, s_scan,
                                   (int)sizeof(s_scan), &n);
    }

    TaiConfigChoice choice = tai_config_choose(ux0, ur0);
    PlatformReadStatus status = choice.status;

    if (choice.path != NULL)
        tai_copy(g_editor.source, sizeof(g_editor.source), choice.path);

    g_editor.source_ok = status == PLATFORM_READ_OK ||
                         status == PLATFORM_READ_TOO_LARGE;

    if (status == PLATFORM_READ_UNREADABLE) {
        /*
         * taiHEN tries ux0: first. If it exists but cannot be read, editing
         * ur0: would write a file the boot never loads. Name the unreadable
         * file and stop instead of silently switching mounts (D71).
         */
        g_editor.unreadable = 1;
        g_editor.read_only = 1;
    }

    if (!g_editor.source_ok) {
        g_editor.read_only = 1;
        g_editor.invalid = 1;

        /*
         * With no live config, any readable snapshot is a recovery source.
         * Probe all three, on both mounts, and remember the mount that owns
         * one. Only probe when no path is already fixed: an unreadable ux0:
         * must not be replaced by a writable ur0: just because ur0: owns a
         * snapshot.
         */
        if (g_editor.source[0] == '\0') {
            static const PlatformBackup KINDS[3] = {
                PLATFORM_BACKUP_ORIGINAL,
                PLATFORM_BACKUP_SESSION,
                PLATFORM_BACKUP_UNDO,
            };
            static const char *const MOUNTS[2] = {
                TAI_CONFIG_UX0,
                TAI_CONFIG_UR0,
            };

            for (int m = 0; m < 2 && g_editor.source[0] == '\0'; ++m) {
                for (int k = 0; k < 3; ++k) {
                    int probe_len = 0;

                    if (platform_read_backup(
                            MOUNTS[m], KINDS[k], s_work,
                            (int)sizeof(s_work),
                            &probe_len) == PLATFORM_READ_OK) {
                        tai_copy(g_editor.source, sizeof(g_editor.source),
                                 MOUNTS[m]);
                        break;
                    }
                }
            }
        }
    } else if (status == PLATFORM_READ_TOO_LARGE) {
        g_editor.too_large = 1;
        g_editor.read_only = 1;
        g_editor.backup_warning = 1;
    } else {
        /*
         * Snapshot the bytes we just read, not a second read: the session
         * backup must be exactly the launch file even if the file changes
         * between the scan and the copy. A failed snapshot is a warning; it
         * must never lock the editor (D61).
         */
        int original_ok = 0;
        int session_ok = 0;

        if (platform_backup_config(g_editor.source, s_scan, n, &original_ok,
                                   &session_ok) != 0 ||
            !original_ok || !session_ok) {
            g_editor.backup_warning = 1;
        }

        /*
         * The card needs the per-snapshot result, not the warning. `.original`
         * is written once, so `original_ok` means "present", while `.session`
         * is written from this launch's bytes.
         */
        g_editor.original_snapshot_present = original_ok;
        g_editor.session_snapshot_written = session_ok;

        memcpy(g_editor.last_text, s_scan, (size_t)n);
        g_editor.last_text[n] = '\0';
        g_editor.last_len = n;
        parse_text(&g_editor, s_scan, n);

        if (g_editor.lines_truncated || g_editor.rows_truncated)
            g_editor.read_only = 1;
    }

    refresh_snapshot_availability();

    /*
     * The card's "original" sentence means "a readable original exists", not
     * "this launch wrote one". The probe above answers exactly that, including
     * the too-large and unreadable paths where the backup is skipped.
     */
    g_editor.original_snapshot_present = g_editor.original_available;

    /*
     * ux0: is tried first, so ur0: can only be active when ux0: is absent: a
     * ur0:-active scan has no shadowed ux0: file to report.
     */
    if (strcmp(g_editor.source, TAI_CONFIG_UX0) == 0 &&
        platform_file_exists(TAI_CONFIG_UR0))
        tai_copy(g_editor.ignored, sizeof(g_editor.ignored), TAI_CONFIG_UR0);

    if (!g_editor.source_ok || g_editor.too_large || g_editor.unreadable ||
        g_editor.lines_truncated || g_editor.rows_truncated ||
        g_editor.truncated) {
        g_editor.orphans_unverified = 1;
    } else {
        for (size_t i = 0; i < sizeof(TAI_DIRS) / sizeof(TAI_DIRS[0]); ++i) {
            OrphanCtx ctx = {&g_editor, TAI_DIRS[i]};
            if (platform_list_dir(TAI_DIRS[i], orphan_cb, &ctx) < 0)
                g_editor.orphans_unverified = 1;
        }

        sort_orphans(&g_editor);
    }
}

int editor_original_snapshot_present(void)
{
    return g_editor.original_snapshot_present;
}

int editor_session_snapshot_written(void)
{
    return g_editor.session_snapshot_written;
}

const char *editor_repair_reason(void)
{
    if (g_editor.write_unrecovered)
        return "REPAIR WITH UNDO FIRST";
    if (g_editor.recover_failed)
        return "RECOVERY FAILED";

    return NULL;
}

const char *editor_readonly_reason(void)
{
    const char *repair = editor_repair_reason();

    if (repair != NULL)
        return repair;
    if (g_editor.unreadable)
        return "CONFIG UNREADABLE";
    if (!g_editor.source_ok)
        return "NO CONFIG.TXT";
    if (g_editor.too_large)
        return "CONFIG TOO LARGE";
    if (g_editor.lines_truncated)
        return "TOO MANY LINES";
    if (g_editor.rows_truncated)
        return "TOO MANY PLUGIN ROWS";
    if (g_editor.read_only)
        return "CONFIG IS READ ONLY";

    return NULL;
}

int editor_can_restore(void)
{
    return g_editor.source[0] != '\0' && !g_editor.unreadable &&
           !g_editor.too_large && !g_editor.lines_truncated &&
           !g_editor.rows_truncated && !g_editor.write_unrecovered &&
           !g_editor.recover_failed;
}

/*
 * A whole-file restore replaces bytes the app never read, so a live file that
 * could not be parsed is not a safe restore target. No config at all is still
 * the recovery case: the snapshot is then the only source.
 */
static const char *restore_blocked_reason(void)
{
    if (g_editor.unreadable)
        return "CONFIG UNREADABLE";
    if (g_editor.too_large)
        return "CONFIG TOO LARGE";
    if (g_editor.lines_truncated)
        return "TOO MANY LINES";
    if (g_editor.rows_truncated)
        return "TOO MANY PLUGIN ROWS";

    return NULL;
}

EditorAction editor_next_action(int row)
{
    if (row < 0 || row >= g_editor.row_count)
        return EDITOR_ACTION_NONE;

    if (editor_readonly_reason() != NULL)
        return EDITOR_ACTION_NONE;

    const EditorRow *r = &g_editor.rows[row];

    if (r->status == EDITOR_BOOT || r->status == EDITOR_NOT_REACHED)
        return EDITOR_ACTION_NONE;

    /*
     * A missing file cannot be enabled. This must come before the locked rule:
     * a locked row that is also missing would otherwise advertise ENABLE and
     * then refuse it.
     */
    if (r->status == EDITOR_MISSING)
        return (r->active_lines > 0 && !r->locked) ? EDITOR_ACTION_DISABLE
                                                   : EDITOR_ACTION_NONE;

    if (r->locked)
        return r->active_lines < r->reached_lines ? EDITOR_ACTION_ENABLE
                                                  : EDITOR_ACTION_NONE;

    if (r->reached_lines == 0)
        return EDITOR_ACTION_NONE;

    /*
     * A mixed row is safer to switch off than to complete: completing it adds
     * a load context the user may not have intended, and the confirm cannot
     * show every context. Disable first; a second press enables all lines.
     */
    if (r->status == EDITOR_PARTIAL)
        return EDITOR_ACTION_DISABLE;

    return r->active_lines == r->reached_lines ? EDITOR_ACTION_DISABLE
                                               : EDITOR_ACTION_ENABLE;
}

const char *editor_status_text(EditorStatus status)
{
    switch (status) {
    case EDITOR_LOADED:
        return "LOADED";
    case EDITOR_DISABLED:
        return "NOT LOADED";
    case EDITOR_MISSING:
        return "MISSING";
    case EDITOR_PARTIAL:
        return "MIXED";
    case EDITOR_NOT_REACHED:
        return "NOT REACHED";
    case EDITOR_BOOT:
        return "BOOT";
    }

    return "UNKNOWN";
}

const char *editor_section_text(PluginSection section)
{
    switch (section) {
    case PLUGIN_SECTION_KERNEL:
        return "KERNEL";
    case PLUGIN_SECTION_SYSTEM:
        return "SYSTEM";
    case PLUGIN_SECTION_SYSTEM_APP:
        return "SYSTEM APPS";
    case PLUGIN_SECTION_ALL_TITLES:
        return "ALL TITLES";
    case PLUGIN_SECTION_TITLE:
        return "TITLES";
    default:
        return "TITLES";
    }
}

const char *editor_row_label(int row)
{
    if (row < 0 || row >= g_editor.row_count)
        return "--";

    const EditorRow *r = &g_editor.rows[row];

    /*
     * Two mounts can hold the same basename. The row then shows the full path
     * so the confirmation names the file that will actually change.
     */
    return r->duplicate_name ? r->path : r->name;
}

static const char *confirm_text_for(const char *name, EditorAction mode)
{
    if (is_storage_driver(name)) {
        return mode == EDITOR_ACTION_DISABLE
                   ? "UX0 MAY NOT MOUNT AFTER THE REBOOT. GAMES AND APPS ON THE CARD MAY NOT START."
                   : "THE STORAGE DRIVER LOADS AGAIN.";
    }

    if (tai_ieq(name, "0syscall6.skprx"))
        return mode == EDITOR_ACTION_DISABLE
                   ? "HIGHER FIRMWARE GAMES MAY FAIL C1-6703-6"
                   : "PATCHES FIRMWARE AND KEY CHECKS AGAIN";
    if (tai_ieq(name, "nonpdrm.skprx"))
        return mode == EDITOR_ACTION_DISABLE
                   ? "NONPDRM INSTALLS FAIL LICENCE CHECKS"
                   : "NONPDRM LICENCES RETURN";
    if (tai_ieq(name, "repatch.skprx"))
        return mode == EDITOR_ACTION_DISABLE
                   ? "MODS IN UX0:REPATCH STOP LOADING"
                   : "MODS LOAD AGAIN";
    if (tai_ieq(name, "nopsmdrm.skprx"))
        return mode == EDITOR_ACTION_DISABLE
                   ? "PSM TITLES FAIL LICENCE CHECKS"
                   : "PSM LICENCES RETURN";
    if (tai_ieq(name, "libshacccg.suprx"))
        return mode == EDITOR_ACTION_DISABLE
                   ? "VITAGL HOMEBREW MAY FAIL TO START"
                   : "SHADER COMPILER LINE RETURNS";
    if (tai_ieq(name, "yamt.suprx"))
        return mode == EDITOR_ACTION_DISABLE ? "YAMT STORAGE MENU DISAPPEARS"
                                            : "YAMT STORAGE MENU RETURNS";
    if (tai_ieq(name, "yamt_helper.skprx"))
        return mode == EDITOR_ACTION_DISABLE
                   ? "YAMT REMOUNT COMMANDS FAIL"
                   : "YAMT REMOUNT COMMANDS RETURN";
    if (tai_ieq(name, "reVitaMotion.suprx"))
        return mode == EDITOR_ACTION_DISABLE
                   ? "REVITA GYRO REMAPPING STOPS"
                   : "REVITA GYRO REMAPPING RETURNS";

    return NULL;
}

const char *editor_confirm_text(int row, EditorAction mode)
{
    if (row < 0 || row >= g_editor.row_count ||
        (mode != EDITOR_ACTION_DISABLE && mode != EDITOR_ACTION_ENABLE))
        return NULL;

    const EditorRow *r = &g_editor.rows[row];

    if (r->locked || r->boot || r->status == EDITOR_NOT_REACHED)
        return NULL;
    if (r->status == EDITOR_MISSING && mode == EDITOR_ACTION_ENABLE)
        return NULL;

    return confirm_text_for(r->name, mode);
}

int editor_row_for_path(const char *path)
{
    return path == NULL ? -1 : row_for_path(&g_editor, path);
}

static int copy_region(char *dst, int out, int cap, const char *src, int from,
                       int to)
{
    for (int i = from; i < to; ++i) {
        if (out >= cap - 1)
            return -1;
        dst[out++] = src[i];
    }

    return out;
}

static void adopt_text(EditorState *e, const char *text, int len)
{
    memcpy(e->last_text, text, (size_t)len);
    e->last_text[len] = '\0';
    e->last_len = len;
    e->write_unrecovered = 0;
    parse_text(e, text, len);
    sync_file_baseline(e);
}

static EditorWriteResult write_failure(void)
{
    if (strcmp(platform_write_error(), "RESTORE") == 0)
        return EDITOR_WRITE_UNRECOVERED;

    return EDITOR_WRITE_FAILED;
}

/*
 * A commit replaces the one-step undo before it touches the live file. Keep
 * the old snapshot in memory so a clean write failure can put it back instead
 * of silently consuming the previous restore point (D70).
 */
static int read_previous_undo(char *buf, int cap, int *len)
{
    return platform_read_backup(g_editor.source, PLATFORM_BACKUP_UNDO, buf,
                                cap, len) == PLATFORM_READ_OK;
}

static int restore_previous_undo(const char *buf, int len)
{
    return platform_snapshot_undo(g_editor.source, buf, len) == 0;
}

static int undo_is_readable(void)
{
    int n = 0;

    return platform_read_backup(g_editor.source, PLATFORM_BACKUP_UNDO, s_scan,
                                (int)sizeof(s_scan), &n) == PLATFORM_READ_OK;
}

static int recover_undo_swap(void)
{
    char path[PLATFORM_PATH_MAX];

    if (snprintf(path, sizeof(path), "%s.undo", g_editor.source) >=
        (int)sizeof(path))
        return 0;

    return platform_recover_swap(path) > 0;
}

/*
 * Copy one source line and, when it belongs to the target row, apply `mode`.
 * Disabling inserts the '#' at the content start; enabling removes the whole
 * leading run so `##path` and `# # path` both come back clean. Everything else,
 * including the line ending, is copied byte for byte.
 */
static int append_line(char *dst, int out, int cap, const char *src,
                       const EditorLine *l, int target, EditorAction mode)
{
    int line_end = l->start + l->len;

    if (target && l->kind == EDITOR_LINE_PATH && l->reached &&
        mode == EDITOR_ACTION_DISABLE) {
        if (l->hash >= 0) {
            out = copy_region(dst, out, cap, src, l->start, line_end);
        } else {
            out = copy_region(dst, out, cap, src, l->start, l->content);
            if (out < 0)
                return -1;
            if (out >= cap - 1)
                return -1;
            dst[out++] = '#';
            out = copy_region(dst, out, cap, src, l->content, line_end);
        }
    } else if (target && l->kind == EDITOR_LINE_PATH && l->reached) {
        if (l->hash >= 0) {
            out = copy_region(dst, out, cap, src, l->start, l->hash);
            if (out < 0)
                return -1;
            out = copy_region(dst, out, cap, src, l->path_start, line_end);
        } else {
            out = copy_region(dst, out, cap, src, l->start, line_end);
        }
    } else {
        out = copy_region(dst, out, cap, src, l->start, line_end);
    }

    if (out < 0)
        return -1;

    return copy_region(dst, out, cap, src, line_end, line_end + l->newline);
}

EditorWriteResult editor_stage_toggle(int row, EditorAction mode, char *err,
                                      int err_cap)
{
    if (err != NULL && err_cap > 0)
        err[0] = '\0';

    if (g_editor.preview_active) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "PREVIEW OPEN");
        return EDITOR_WRITE_REFUSED;
    }

    if (row < 0 || row >= g_editor.row_count ||
        (mode != EDITOR_ACTION_DISABLE && mode != EDITOR_ACTION_ENABLE)) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "NO ACTION");
        return EDITOR_WRITE_REFUSED;
    }

    const char *read_only = editor_readonly_reason();
    if (read_only != NULL) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "%s", read_only);
        return EDITOR_WRITE_REFUSED;
    }

    EditorRow *r = &g_editor.rows[row];

    if (r->locked && mode == EDITOR_ACTION_DISABLE) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "HENKAKU IS LOCKED");
        return EDITOR_WRITE_REFUSED;
    }

    if (r->boot || r->status == EDITOR_NOT_REACHED) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "ROW IS NOT EDITABLE");
        return EDITOR_WRITE_REFUSED;
    }

    if (r->status == EDITOR_MISSING && mode == EDITOR_ACTION_ENABLE) {
        /*
         * Presence is the scan-time cache. A stage never stats the card, so a
         * file moved in over FTP is not trusted against a stale MISSING row;
         * the commit's stale check still guards the bytes.
         */
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "FILE IS MISSING");
        return EDITOR_WRITE_REFUSED;
    }

    /*
     * A caller may not skip the policy: a PARTIAL row disables first, and an
     * active row cannot be "enabled" again. Require the exact next action.
     */
    EditorAction next = editor_next_action(row);

    if (next == EDITOR_ACTION_NONE || mode != next) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "NO ACTION");
        return EDITOR_WRITE_REFUSED;
    }

    int out = 0;

    for (int i = 0; i < g_editor.line_count; ++i) {
        out = append_line(s_work, out, (int)sizeof(s_work), g_editor.text,
                          &g_editor.lines[i], g_editor.lines[i].row == row,
                          mode);
        if (out < 0)
            break;
    }

    if (out < 0 || out >= EDITOR_TEXT_MAX - 1) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "CONFIG TOO LARGE TO EDIT");
        return EDITOR_WRITE_REFUSED;
    }

    s_work[out] = '\0';

    parse_text(&g_editor, s_work, out);

    return EDITOR_WRITE_OK;
}

void editor_discard_draft(void)
{
    if (g_editor.preview_active)
        return;

    if (!g_editor.draft_dirty)
        return;

    parse_text(&g_editor, g_editor.last_text, g_editor.last_len);
}


/*
 * The one config-write protocol shared by the draft and the restore preview.
 * The public commits own their guards and their post-commit meaning; this
 * helper owns the stale check, the optional `.undo` snapshot, the verified
 * write, the failure mapping, and the baseline adoption (KTD5).
 */
typedef struct {
    int had_live;      /* the scan had a readable live file */
    int recovering;    /* write_unrecovered: the live file is the damaged copy */
    int snapshot_live; /* take `.undo` from last_text before the write */
    int keep_previous; /* restore the old `.undo` on a clean failure */
    int clear_preview; /* clear the preview before adopting the text */
    int kind;          /* EDITOR_SNAPSHOT_*, or -1 for a draft */
} CommitPlan;

static EditorWriteResult commit_text(const char *text, int len,
                                     const CommitPlan *plan, char *err,
                                     int err_cap)
{
    /*
     * The commit was built from the scan baseline. If the card changed behind
     * it, the commit is not permission to overwrite it (R5). When scan found
     * no live file, a file that appeared since is just as stale; only an
     * absent file is the recovery case. An unrecovered write is the exception:
     * the live file is the damaged copy, and `.undo` is the known-good source.
     */
    if (plan->had_live && !plan->recovering) {
        int checked = 0;

        if (platform_read_config(g_editor.source, s_work,
                                 (int)sizeof(s_work),
                                 &checked) != PLATFORM_READ_OK ||
            !matches_last_text(s_work, checked)) {
            if (err != NULL && err_cap > 0)
                snprintf(err, (size_t)err_cap,
                         "CONFIG CHANGED ON DISK. RESTART COCKPIT.");
            return EDITOR_WRITE_REFUSED;
        }
    } else if (!g_editor.unreadable && !plan->recovering) {
        int checked = 0;

        if (platform_read_config(g_editor.source, s_work,
                                 (int)sizeof(s_work),
                                 &checked) != PLATFORM_READ_ABSENT) {
            if (err != NULL && err_cap > 0)
                snprintf(err, (size_t)err_cap,
                         "CONFIG CHANGED ON DISK. RESTART COCKPIT.");
            return EDITOR_WRITE_REFUSED;
        }
    }

    int previous_undo_len = 0;
    int have_previous_undo = 0;

    if (plan->snapshot_live) {
        have_previous_undo =
            read_previous_undo(s_work, (int)sizeof(s_work),
                               &previous_undo_len);

        /*
         * Keep the pre-write bytes as the one-step undo. This must exist before
         * the live file changes; if it cannot be written, nothing is committed
         * (D70).
         */
        if (platform_snapshot_undo(g_editor.source, g_editor.last_text,
                                   g_editor.last_len) != 0) {
            if (err != NULL && err_cap > 0)
                snprintf(err, (size_t)err_cap, "WRITE FAILED UNDO %s",
                         platform_write_error());

            /*
             * The snapshot write can fail before the old `.undo` is touched
             * (DATA/OPEN/VERIFY) or after it was moved aside (MOVE/RENAME/
             * RESTORE). Never destroy the one-step restore point: probe the
             * live `.undo` first, recover an interrupted `.undo.old` swap, then
             * fall back to rewriting the bytes we read.
             */
            if (have_previous_undo && !undo_is_readable()) {
                if (recover_undo_swap())
                    have_previous_undo = 1;
                else
                    have_previous_undo =
                        restore_previous_undo(s_work, previous_undo_len);
            }

            g_editor.undo_available = have_previous_undo;
            return EDITOR_WRITE_FAILED;
        }
    }

    int undo_differs = !matches_last_text(text, len);

    if (platform_write_config(g_editor.source, text, len) != 0) {
        EditorWriteResult result = write_failure();

        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "WRITE FAILED %s",
                     platform_write_error());

        if (result == EDITOR_WRITE_UNRECOVERED) {
            g_editor.write_unrecovered = 1;
        } else if (plan->keep_previous && have_previous_undo) {
            /* A clean failure changed no config byte: keep the old undo. */
            have_previous_undo =
                restore_previous_undo(s_work, previous_undo_len);
        }

        /*
         * A restore's `.undo` now holds the recovered live bytes, so it is not
         * a different state. An unrecovered write leaves that snapshot as the
         * only good copy, so keep the undo offer.
         */
        if (plan->kind != EDITOR_SNAPSHOT_UNDO)
            g_editor.undo_available =
                have_previous_undo || result == EDITOR_WRITE_UNRECOVERED;

        return result;
    }

    if (plan->clear_preview)
        clear_preview_state(&g_editor);

    adopt_text(&g_editor, text, len);

    /*
     * The write replaced whatever condition made the live file uneditable.
     * The parsed text sets lines_truncated/rows_truncated again if it has its
     * own problem, but the preview refused that case before this call.
     */
    g_editor.source_ok = 1;
    g_editor.read_only = 0;
    g_editor.unreadable = 0;
    g_editor.too_large = 0;

    if (plan->kind == EDITOR_SNAPSHOT_UNDO) {
        /*
         * One step only: remove the source after the verified write. If the
         * removal fails, keep reporting the surviving `.undo` so the dock does
         * not claim a clean card while the old bytes remain.
         */
        g_editor.undo_available =
            platform_remove_undo(g_editor.source) != 0;
    } else if (plan->had_live) {
        g_editor.undo_available = undo_differs;
    }

    return EDITOR_WRITE_OK;
}

EditorWriteResult editor_commit_draft(char *err, int err_cap)
{
    if (err != NULL && err_cap > 0)
        err[0] = '\0';

    if (g_editor.preview_active) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "PREVIEW OPEN");
        return EDITOR_WRITE_REFUSED;
    }

    if (!g_editor.draft_dirty) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "NO CHANGES");
        return EDITOR_WRITE_REFUSED;
    }

    const char *read_only = editor_readonly_reason();
    if (read_only != NULL) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "%s", read_only);
        return EDITOR_WRITE_REFUSED;
    }

    CommitPlan plan;
    plan.had_live = 1;
    plan.recovering = 0;
    plan.snapshot_live = 1;
    plan.keep_previous = 1;
    plan.clear_preview = 0;
    plan.kind = -1;

    return commit_text(g_editor.text, g_editor.text_len, &plan, err, err_cap);
}

int editor_draft_dirty(void)
{
    return g_editor.draft_dirty;
}

int editor_draft_count(void)
{
    return g_editor.draft_count;
}

int editor_draft_change(int row)
{
    if (row < 0 || row >= g_editor.row_count)
        return 0;

    return g_editor.rows[row].draft_change;
}

static int platform_backup_kind(EditorSnapshot kind, PlatformBackup *out)
{
    switch (kind) {
    case EDITOR_SNAPSHOT_ORIGINAL:
        *out = PLATFORM_BACKUP_ORIGINAL;
        return 1;
    case EDITOR_SNAPSHOT_SESSION:
        *out = PLATFORM_BACKUP_SESSION;
        return 1;
    case EDITOR_SNAPSHOT_UNDO:
        *out = PLATFORM_BACKUP_UNDO;
        return 1;
    }

    return 0;
}

static void mark_snapshot_unavailable(EditorSnapshot kind)
{
    switch (kind) {
    case EDITOR_SNAPSHOT_ORIGINAL:
        g_editor.original_available = 0;
        break;
    case EDITOR_SNAPSHOT_SESSION:
        g_editor.session_available = 0;
        break;
    case EDITOR_SNAPSHOT_UNDO:
        g_editor.undo_available = 0;
        break;
    }
}

/*
 * A successful read is the freshest availability fact. For `.undo` the dock
 * also needs the truth table's "differs from live" test; the bytes are already
 * in the working model, so this needs no second read.
 */
static void mark_snapshot_available(EditorSnapshot kind)
{
    switch (kind) {
    case EDITOR_SNAPSHOT_ORIGINAL:
        g_editor.original_available = 1;
        break;
    case EDITOR_SNAPSHOT_SESSION:
        g_editor.session_available = 1;
        break;
    case EDITOR_SNAPSHOT_UNDO:
        g_editor.undo_available =
            !g_editor.source_ok ||
            !matches_last_text(g_editor.text, g_editor.text_len);
        break;
    }
}

const char *editor_diff_text(EditorDiff diff)
{
    switch (diff) {
    case EDITOR_DIFF_ENABLE:
        return "WILL ENABLE";
    case EDITOR_DIFF_DISABLE:
        return "WILL DISABLE";
    case EDITOR_DIFF_MOVE:
        return "WILL MOVE";
    case EDITOR_DIFF_REMOVE:
        return "WILL REMOVE";
    case EDITOR_DIFF_UNCHANGED:
    default:
        return NULL;
    }
}

static const EditorWorkRow *preview_work_for_path(const char *path)
{
    for (int i = 0; i < g_editor.preview_work_count; ++i) {
        if (tai_ieq(s_preview_work[i].path, path))
            return &s_preview_work[i];
    }

    return NULL;
}

EditorDiff editor_preview_diff(int row)
{
    if (!g_editor.preview_active || row < 0 || row >= g_editor.row_count)
        return EDITOR_DIFF_UNCHANGED;

    int after = g_editor.rows[row].active_lines;
    int before = g_editor.rows[row].preview_before;

    if (after > before)
        return EDITOR_DIFF_ENABLE;
    if (after < before)
        return EDITOR_DIFF_DISABLE;

    const EditorWorkRow *w = preview_work_for_path(g_editor.rows[row].path);

    if (w != NULL && w->section != g_editor.rows[row].section)
        return EDITOR_DIFF_MOVE;

    return EDITOR_DIFF_UNCHANGED;
}

EditorWriteResult editor_preview_restore(EditorSnapshot kind, char *err,
                                         int err_cap)
{
    if (err != NULL && err_cap > 0)
        err[0] = '\0';

    if (g_editor.preview_active) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "PREVIEW OPEN");
        return EDITOR_WRITE_REFUSED;
    }

    if (g_editor.source[0] == '\0') {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "NO CONFIG.TXT");
        return EDITOR_WRITE_REFUSED;
    }

    /*
     * A snapshot restore is a whole-file replace. Refuse it when the live file
     * could not be read or modelled: the pre-restore `.undo` would be empty or
     * truncated, and the bytes being replaced were never seen. Undo is the
     * recovery path for those states and stays available.
     */
    if (kind != EDITOR_SNAPSHOT_UNDO) {
        const char *blocked = restore_blocked_reason();

        if (blocked != NULL) {
            if (err != NULL && err_cap > 0)
                snprintf(err, (size_t)err_cap, "%s", blocked);
            return EDITOR_WRITE_REFUSED;
        }

        if (g_editor.write_unrecovered) {
            if (err != NULL && err_cap > 0)
                snprintf(err, (size_t)err_cap, "REPAIR WITH UNDO FIRST");
            return EDITOR_WRITE_REFUSED;
        }
    }

    PlatformBackup backup;

    if (!platform_backup_kind(kind, &backup)) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "NO RESTORE POINT");
        return EDITOR_WRITE_REFUSED;
    }

    /*
     * Read the selected snapshot once. This is the only card access on the
     * preview path; the parsed copy is what a confirm writes.
     */
    int n = 0;
    PlatformReadStatus read = platform_read_backup(
        g_editor.source, backup, s_undo, (int)sizeof(s_undo), &n);

    if (read != PLATFORM_READ_OK) {
        mark_snapshot_unavailable(kind);

        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "%s",
                     read == PLATFORM_READ_TOO_LARGE
                         ? "RESTORE POINT TOO LARGE"
                         : "NO RESTORE POINT");
        return EDITOR_WRITE_REFUSED;
    }

    /*
     * Keep the working set the diff is against. A draft is the working set
     * when one exists; the file-row cache would answer the wrong question here.
     */
    memcpy(s_preview_saved_work, g_editor.text,
           (size_t)g_editor.text_len);
    g_editor.preview_saved_work_len = g_editor.text_len;
    g_editor.preview_saved_dirty = g_editor.draft_dirty;

    clear_preview_state(&g_editor);
    g_editor.preview_discards_draft = g_editor.draft_dirty;
    g_editor.preview_kind = kind;

    for (int i = 0; i < g_editor.row_count; ++i) {
        EditorWorkRow *w = &s_preview_work[i];

        tai_copy(w->path, sizeof(w->path), g_editor.rows[i].path);
        w->active_lines = g_editor.rows[i].active_lines;
        w->section = g_editor.rows[i].section;
        ++g_editor.preview_work_count;
    }

    g_editor.preview_active = 1;

    parse_text(&g_editor, s_undo, n);

    /*
     * The preview replaces the whole file, so a model that could not represent
     * every line or row cannot prove the locked-path safeguard. Fail closed
     * rather than offer a commit with an incomplete diff.
     */
    if (g_editor.lines_truncated || g_editor.rows_truncated) {
        restore_saved_work(&g_editor);
        mark_snapshot_unavailable(kind);

        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "RESTORE POINT TOO COMPLEX");
        return EDITOR_WRITE_REFUSED;
    }

    mark_snapshot_available(kind);

    for (int i = 0; i < g_editor.row_count; ++i) {
        EditorRow *r = &g_editor.rows[i];
        int before = 0;
        PluginSection before_section = r->section;

        for (int j = 0; j < g_editor.preview_work_count; ++j) {
            if (tai_ieq(s_preview_work[j].path, r->path)) {
                before = s_preview_work[j].active_lines;
                before_section = s_preview_work[j].section;
                break;
            }
        }

        r->preview_before = before;

        if (r->section != before_section) {
            g_editor.preview_section_change = 1;
            if (r->section == PLUGIN_SECTION_KERNEL ||
                before_section == PLUGIN_SECTION_KERNEL)
                g_editor.preview_kernel_change = 1;
        }

        if (r->active_lines > before)
            ++g_editor.preview_enable_total;
        else if (r->active_lines < before)
            ++g_editor.preview_disable_total;
    }

    for (int i = 0; i < g_editor.preview_work_count; ++i) {
        const EditorWorkRow *w = &s_preview_work[i];

        if (row_for_path(&g_editor, w->path) >= 0)
            continue;

        if (w->section == PLUGIN_SECTION_KERNEL)
            g_editor.preview_kernel_change = 1;

        if (g_editor.preview_removed_count < EDITOR_REMOVALS_MAX) {
            tai_copy(s_preview_removed[g_editor.preview_removed_count++],
                     sizeof(s_preview_removed[0]), w->path);
        }

        if (!g_editor.preview_locked_removal &&
            is_protected_path(&g_editor, w->path)) {
            g_editor.preview_locked_removal = 1;
            tai_copy(g_editor.preview_locked_path,
                     sizeof(g_editor.preview_locked_path), w->path);
        }
    }

    /*
     * A protected path can stay in the file and still be switched off: every
     * line commented out, or a `*!` halt placed before it. Either way the
     * plugin no longer loads, and that functional removal must not bypass the
     * BOOT/locked-HENKAKU rule. Keying on `active_lines == 0` alone covers both:
     * a commented path is still reached, while a halted path is not.
     */
    for (int i = 0; i < g_editor.row_count; ++i) {
        const EditorRow *r = &g_editor.rows[i];

        if (!g_editor.preview_locked_removal &&
            (r->boot || r->locked) && r->active_lines == 0) {
            g_editor.preview_locked_removal = 1;
            tai_copy(g_editor.preview_locked_path,
                     sizeof(g_editor.preview_locked_path), r->path);
        }
    }

    int changed = g_editor.preview_removed_count > 0 ||
                  g_editor.preview_enable_total > 0 ||
                  g_editor.preview_disable_total > 0 ||
                  g_editor.preview_section_change;

    /*
     * An undo after an unrecovered write repairs an absent or partial live
     * file. For a plain undo, compare the snapshot bytes to the live baseline:
     * a comment-only difference is still a real change, while a byte-identical
     * snapshot has nothing to write. A row-level comparison cannot see that.
     */
    if (kind == EDITOR_SNAPSHOT_UNDO)
        g_editor.preview_no_change =
            matches_last_text(g_editor.text, g_editor.text_len) &&
            !g_editor.write_unrecovered;
    else
        g_editor.preview_no_change = !changed;
    refresh_batch_warning(&g_editor, 1);

    return EDITOR_WRITE_OK;
}

void editor_cancel_preview(void)
{
    if (g_editor.preview_active)
        restore_saved_work(&g_editor);
}

EditorWriteResult editor_commit_preview(char *err, int err_cap)
{
    if (err != NULL && err_cap > 0)
        err[0] = '\0';

    if (!g_editor.preview_active) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "NO PREVIEW");
        return EDITOR_WRITE_REFUSED;
    }

    if (g_editor.preview_locked_removal) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "RESTORE DROPS %s",
                     g_editor.preview_locked_path);
        return EDITOR_WRITE_REFUSED;
    }

    if (g_editor.preview_no_change) {
        if (err != NULL && err_cap > 0)
            snprintf(err, (size_t)err_cap, "NO CHANGES");
        return EDITOR_WRITE_REFUSED;
    }

    int kind = g_editor.preview_kind;

    if (kind != EDITOR_SNAPSHOT_UNDO) {
        const char *blocked = restore_blocked_reason();

        if (blocked != NULL) {
            if (err != NULL && err_cap > 0)
                snprintf(err, (size_t)err_cap, "%s", blocked);
            return EDITOR_WRITE_REFUSED;
        }
    }

    CommitPlan plan;
    plan.had_live = g_editor.source_ok;
    plan.recovering = g_editor.write_unrecovered || g_editor.too_large;
    plan.snapshot_live =
        kind != EDITOR_SNAPSHOT_UNDO && g_editor.source_ok;
    plan.keep_previous = kind != EDITOR_SNAPSHOT_UNDO;
    plan.clear_preview = 1;
    plan.kind = kind;

    return commit_text(g_editor.text, g_editor.text_len, &plan, err, err_cap);
}

int editor_snapshot_available(EditorSnapshot kind)
{
    switch (kind) {
    case EDITOR_SNAPSHOT_ORIGINAL:
        return g_editor.original_available;
    case EDITOR_SNAPSHOT_SESSION:
        return g_editor.session_available;
    case EDITOR_SNAPSHOT_UNDO:
        return g_editor.undo_available;
    }

    return 0;
}

int editor_undo_available(void)
{
    return g_editor.undo_available;
}

int editor_launch_active_for_path(const char *path)
{
    if (path == NULL)
        return -1;

    for (int i = 0; i < s_file_row_count; ++i) {
        if (tai_ieq(s_file_rows[i].path, path))
            return s_file_rows[i].launch_active;
    }

    return -1;
}

int editor_preview_active(void)
{
    return g_editor.preview_active;
}

int editor_preview_no_change(void)
{
    return g_editor.preview_no_change;
}

int editor_preview_discards_draft(void)
{
    return g_editor.preview_discards_draft;
}

int editor_preview_locked_removal(void)
{
    return g_editor.preview_locked_removal;
}

const char *editor_preview_locked_path(void)
{
    return g_editor.preview_locked_removal ? g_editor.preview_locked_path
                                           : NULL;
}

const char *editor_batch_warning(void)
{
    return g_editor.batch_warning_set ? g_editor.batch_warning : NULL;
}

int editor_preview_removed_count(void)
{
    return g_editor.preview_removed_count;
}

const char *editor_preview_removed_path(int index)
{
    if (index < 0 || index >= g_editor.preview_removed_count)
        return NULL;

    return s_preview_removed[index];
}
