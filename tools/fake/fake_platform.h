#ifndef COCKPIT_TOOLS_FAKE_PLATFORM_H
#define COCKPIT_TOOLS_FAKE_PLATFORM_H

#include <stdint.h>

/*
 * A controllable implementation of core/platform.h, shared by tools/preview and
 * tools/test.
 *
 * This is the test double the port exists for (D7). Scenario control lives
 * here, so the preview drives the real sampler through the real policy instead
 * of writing g_metrics by hand -- which is what made the old preview prove
 * nothing about validity propagation (D26).
 */

/* Healthy console: ux0 present, uma0 absent, 182 MB free user. */
void fake_reset(void);

void fake_set_memory_mb(int user_mb);
void fake_set_memory_absent(void);

void fake_set_cpu_absent(void);
void fake_set_cpu_usage(int core, int percent);
void fake_set_cpu_mask(unsigned mask);
void fake_set_cpu_activity(int ctx_per_s, int wake_per_s);
void fake_set_cpu_totals(uint64_t ctx_switches, uint64_t wakeups);
void fake_set_cpu_broken(int on);

void fake_set_power(int percent, int charging);
void fake_set_power_absent(void);
void fake_set_power_partial(int temp_ok, int volt_ok, int cycles_ok,
                            int health_ok);

void fake_set_storage(int index, uint64_t total_bytes, uint64_t free_bytes);
void fake_set_storage_absent(int index);
void fake_set_storage_error(int index, int error);

void fake_set_identity_absent(void);
void fake_set_clock_absent(void);

void fake_add_file(const char *path, const char *text);
void fake_remove_file(const char *path);
void fake_fail_backup(int on);
void fake_fail_read(int on);
void fake_fail_read_path(const char *path, int on);
void fake_fail_write_path(const char *path, int on);

/*
 * Instrumentation for the editor tests: platform_write_config calls and
 * platform_file_exists calls since fake_reset. They prove a stage or discard
 * touches neither the card nor its presence cache. `fake_backup_read_count`
 * counts platform_read_backup calls, so a render path that reads a snapshot is
 * visible.
 */
int fake_write_count(void);
int fake_stat_count(void);
int fake_backup_read_count(void);

typedef enum {
    FAKE_RENAME_NONE = 0,
    FAKE_RENAME_FINAL = 1,
    FAKE_RENAME_RESTORE = 2,
} FakeRenameStage;

void fake_fail_rename_path(const char *path, int stage);
void fake_fail_remove_path(const char *path, int on);
void fake_fail_recover(int on);
void fake_fail_list_dir(int on);

#endif
