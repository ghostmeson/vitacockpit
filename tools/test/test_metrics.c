#include <stdio.h>
#include <string.h>

#include "core/history.h"
#include "core/metrics.h"
#include "core/platform.h"
#include "core/plugins.h"
#include "fake_platform.h"

/*
 * Host tests for the sampling policy.
 *
 * This is the part of COCKPIT that can be exercised without a console, which is
 * the whole reason core/platform.h exists (D7). Until these existed, the alert
 * policy was executed by nothing: deleting every arm of update_alert() left the
 * preview PNGs byte-identical (review finding #1).
 *
 * Not linked into cockpit.self. Run with tools/test/run.sh.
 */

#define GB (1024ull * 1024ull * 1024ull)
#define MB (1024ull * 1024ull)

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                      \
    do {                                                                 \
        ++g_checks;                                                      \
        if (!(cond)) {                                                   \
            ++g_failures;                                                \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                \
    } while (0)

/* The hazard band is the only failure signal the app has (D15, D23). */
static void test_alert_policy(void)
{
    fake_reset();
    fake_set_power(20, 0);
    metrics_init();
    CHECK(g_metrics.alert == 0);

    fake_reset();
    fake_set_power(19, 0);
    metrics_init();
    CHECK(g_metrics.alert == 1);

    fake_reset();
    fake_set_power(19, 1);
    metrics_init();
    CHECK(g_metrics.alert == 0);

    fake_reset();
    fake_set_storage_absent(PLATFORM_STORAGE_UX0);
    metrics_init();
    CHECK(g_metrics.alert == 1);

    fake_reset();
    fake_set_memory_absent();
    metrics_init();
    CHECK(g_metrics.alert == 1);

    /* A failed power read is its own fault, not a silent all-clear (D23). */
    fake_reset();
    fake_set_power_absent();
    metrics_init();
    CHECK(g_metrics.alert == 1);
}

/*
 * The CPU trace is system-wide, not this process's share. The fake emits raw
 * idle-clock deltas; the policy must turn them into stable per-core numbers.
 */
static void test_cpu_usage(void)
{
    fake_reset();
    fake_set_cpu_activity(900, 250);
    metrics_init(); /* first sample establishes the baseline */
    metrics_sample_slow();

    CHECK(g_metrics.cpu_valid == 1);
    CHECK(g_metrics.cpu_usage_pct == 13); /* (18+22+9+4)/4, rounded down */
    CHECK(g_metrics.cpu_core_pct[0] == 18);
    CHECK(g_metrics.cpu_core_pct[1] == 22);
    CHECK(g_metrics.cpu_core_pct[2] == 9);
    CHECK(g_metrics.cpu_core_pct[3] == 4);
    CHECK(g_metrics.cpu_ctx_valid == 1);
    CHECK(g_metrics.cpu_ctx_per_s == 900);
    CHECK(g_metrics.cpu_wake_valid == 1);
    CHECK(g_metrics.cpu_wake_per_s == 250);
    CHECK(g_metrics.hist_cpu.len == 1);
    CHECK(history_at(&g_metrics.hist_cpu, 0) == 13.0f);
}

/* Zero-filled counters are absent, not a measured 0/s. The idle clocks are
 * independent, so the CPU load itself stays valid. */
static void test_cpu_rates_absent(void)
{
    fake_reset();
    fake_set_cpu_activity(0, 0);
    metrics_init();
    metrics_sample_slow();

    CHECK(g_metrics.cpu_valid == 1);
    CHECK(g_metrics.cpu_ctx_valid == 0);
    CHECK(g_metrics.cpu_wake_valid == 0);
}

/* Frozen counters are not a measured zero either. */
static void test_cpu_rates_frozen(void)
{
    fake_reset();
    metrics_init();
    metrics_sample_slow();
    CHECK(g_metrics.cpu_ctx_valid == 1);
    CHECK(g_metrics.cpu_wake_valid == 1);

    fake_set_cpu_activity(0, 0);
    metrics_sample_slow();

    CHECK(g_metrics.cpu_ctx_valid == 0);
    CHECK(g_metrics.cpu_wake_valid == 0);
}

/* A shrinking active mask or a 32-bit wrap makes the summed total regress. */
static void test_cpu_rates_regress(void)
{
    fake_reset();
    metrics_init();
    metrics_sample_slow();

    fake_set_cpu_activity(0, 0);
    fake_set_cpu_totals(10, 5);
    metrics_sample_slow();

    CHECK(g_metrics.cpu_ctx_valid == 0);
    CHECK(g_metrics.cpu_wake_valid == 0);
}

/* One counter can advance while the other is frozen; the rates are independent. */
static void test_cpu_rates_independent(void)
{
    fake_reset();
    metrics_init();
    metrics_sample_slow();

    fake_set_cpu_activity(840, 0);
    metrics_sample_slow();

    CHECK(g_metrics.cpu_ctx_valid == 1);
    CHECK(g_metrics.cpu_ctx_per_s == 840);
    CHECK(g_metrics.cpu_wake_valid == 0);
}

/* An inactive core is unknown, not 0 %, and the average covers only active
 * cores. */
static void test_cpu_active_mask(void)
{
    fake_reset();
    fake_set_cpu_mask(0x5); /* cores 0 and 2 */
    fake_set_cpu_usage(0, 50);
    fake_set_cpu_usage(1, 0);
    fake_set_cpu_usage(2, 100);
    fake_set_cpu_usage(3, 0);

    metrics_init();
    metrics_sample_slow();

    CHECK(g_metrics.cpu_valid == 1);
    CHECK(g_metrics.cpu_core_pct[0] == 50);
    CHECK(g_metrics.cpu_core_pct[1] == CPU_CORE_UNKNOWN);
    CHECK(g_metrics.cpu_core_pct[2] == 100);
    CHECK(g_metrics.cpu_core_pct[3] == CPU_CORE_UNKNOWN);
    CHECK(g_metrics.cpu_usage_pct == 75);
}

/* A delta larger than elapsed time is a unit mismatch, not a 0 % core. */
static void test_cpu_bad_unit_fails_closed(void)
{
    fake_reset();
    metrics_init();
    metrics_sample_slow();

    int len_before = g_metrics.hist_cpu.len;
    fake_set_cpu_broken(1);
    metrics_sample_slow();

    CHECK(g_metrics.cpu_valid == 0);
    CHECK(g_metrics.cpu_core_pct[0] == CPU_CORE_UNKNOWN);
    CHECK(g_metrics.hist_cpu.len == len_before);
}

/* A failed CPU read leaves the old trace untouched and renders "--". */
static void test_cpu_failed_read_leaves_trace_alone(void)
{
    fake_reset();
    metrics_init();
    metrics_sample_slow();

    int len_before = g_metrics.hist_cpu.len;
    CHECK(len_before > 0);
    CHECK(g_metrics.cpu_valid == 1);

    fake_set_cpu_absent();
    metrics_sample_slow();

    CHECK(g_metrics.cpu_valid == 0);
    CHECK(g_metrics.cpu_core_pct[0] == CPU_CORE_UNKNOWN);
    CHECK(g_metrics.cpu_ctx_valid == 0);
    CHECK(g_metrics.cpu_wake_valid == 0);
    CHECK(g_metrics.hist_cpu.len == len_before);
}

/* Unit conversion: the [C] storage risk, now pinned. */
static void test_storage_conversion(void)
{
    fake_reset();
    fake_set_storage(PLATFORM_STORAGE_UX0, 32ull * GB, 20ull * GB);
    metrics_init();

    CHECK(g_metrics.storage[PLATFORM_STORAGE_UX0].valid == 1);
    CHECK(g_metrics.storage[PLATFORM_STORAGE_UX0].total_mb == 32768);
    CHECK(g_metrics.storage[PLATFORM_STORAGE_UX0].used_mb == 12288);

    /* A sub-gigabyte volume keeps its MB precision, so the panel can print
     * "384/512 MB" instead of a rounded "0.4/0.5 GB". */
    fake_reset();
    fake_set_storage(PLATFORM_STORAGE_UX0, 512ull * MB, 128ull * MB);
    metrics_init();
    CHECK(g_metrics.storage[PLATFORM_STORAGE_UX0].total_mb == 512);
    CHECK(g_metrics.storage[PLATFORM_STORAGE_UX0].used_mb == 384);

    /* Absent mount is absent, not zero. */
    fake_reset();
    fake_set_storage_absent(PLATFORM_STORAGE_UMA0);
    metrics_init();
    CHECK(g_metrics.storage[PLATFORM_STORAGE_UMA0].valid == 0);
}

static void test_frame_history_needs_a_frame(void)
{
    fake_reset();
    metrics_init();
    CHECK(g_metrics.hist_frame.len == 0);

    metrics_sample_slow();
    CHECK(g_metrics.hist_frame.len == 0);

    metrics_sample_frame(0.016f);
    metrics_sample_slow();
    CHECK(g_metrics.hist_frame.len == 1);
    CHECK(history_at(&g_metrics.hist_frame, 0) > 0.0f);
}

static void test_power_field_validity(void)
{
    fake_reset();
    metrics_init();

    CHECK(g_metrics.power_valid == 1);
    CHECK(g_metrics.batt_temp_valid == 1);
    CHECK(g_metrics.batt_mv_valid == 1);
    CHECK(g_metrics.batt_cycles_valid == 1);
    CHECK(g_metrics.batt_health_valid == 1);

    fake_reset();
    fake_set_power_partial(0, 1, 1, 0);
    metrics_init();
    CHECK(g_metrics.power_valid == 1);
    CHECK(g_metrics.batt_temp_valid == 0);
    CHECK(g_metrics.batt_mv_valid == 1);
    CHECK(g_metrics.batt_cycles_valid == 1);
    CHECK(g_metrics.batt_health_valid == 0);
}

static void test_storage_error_is_carried(void)
{
    fake_reset();
    fake_set_storage_error(PLATFORM_STORAGE_UX0, (int)0x80010030u);
    metrics_init();

    CHECK(g_metrics.storage[PLATFORM_STORAGE_UX0].valid == 0);
    CHECK(g_metrics.storage[PLATFORM_STORAGE_UX0].error == (int)0x80010030u);
    CHECK(g_metrics.storage[PLATFORM_STORAGE_UX0].errno_byte == 0x30);
}

static void test_memory_conversion(void)
{
    fake_reset();
    fake_set_memory_mb(182);
    metrics_init();

    CHECK(g_metrics.mem_valid == 1);
    CHECK(g_metrics.mem_user_mb == 182);
    CHECK(g_metrics.mem_cdram_mb == 41);
    CHECK(g_metrics.mem_phy_mb == 64);
}

static void test_history_ring(void)
{
    History h;

    history_reset(&h);
    CHECK(h.len == 0);
    CHECK(history_at(&h, 0) == 0.0f);

    for (int i = 0; i < 5; ++i)
        history_push(&h, (float)i);

    CHECK(h.len == 5);
    CHECK(history_at(&h, 0) == 4.0f); /* newest first */
    CHECK(history_at(&h, 4) == 0.0f);

    for (int i = 5; i < HISTORY_CAP + 3; ++i)
        history_push(&h, (float)i);

    CHECK(h.len == HISTORY_CAP);
    CHECK(history_at(&h, 0) == (float)(HISTORY_CAP + 2));
    CHECK(history_at(&h, HISTORY_CAP) == 0.0f); /* out of range */
}

static void test_frame_floor(void)
{
    fake_reset();
    metrics_init();
    metrics_sample_frame(0.0f);
    float zero_dt = g_metrics.frame_ms;

    /* A negative dt is treated exactly as zero, not as a huge frame. */
    fake_reset();
    metrics_init();
    metrics_sample_frame(-1.0f);
    CHECK(g_metrics.frame_ms == zero_dt);

    /* The floor holds after enough zero-dt frames to decay the EMA to nothing. */
    fake_reset();
    metrics_init();
    for (int i = 0; i < 200; ++i)
        metrics_sample_frame(0.0f);
    CHECK(g_metrics.frame_ms >= 0.1f);

    /* A 33 ms frame is the peak the FRAME trace shows, not the EMA. */
    fake_reset();
    metrics_init();
    metrics_sample_frame(0.033f);
    metrics_sample_slow();
    CHECK(history_at(&g_metrics.hist_frame, 0) >= 33.0f);
}

/* tai config parsing: "listed" and "loaded" are different questions. */
static int section_of(const PluginList *list, const char *name);

static void test_plugins_parse(void)
{
    static const char CFG[] = "# comment\n"
                              "*KERNEL\n"
                              "ur0:tai/kubridge.skprx\n"
                              "  ux0:tai/nonpdrm.skprx  \r\n"
                              "*main\n"
                              "ur0:tai/libshacccg.suprx\n"
                              "#ur0:tai/vitaos_ps.suprx\n"
                              "not-a-plugin.txt\n"
                              "ur0:tai/kubridge.skprx\n";

    PluginList list;
    memset(&list, 0, sizeof(list));

    int added = plugins_parse(CFG, &list);

    CHECK(added == 3);
    CHECK(list.count == 3);

    CHECK(list.items[0].section == PLUGIN_SECTION_KERNEL);
    CHECK(list.items[1].section == PLUGIN_SECTION_KERNEL);
    CHECK(list.items[2].section == PLUGIN_SECTION_SYSTEM);

    /* A commented-out entry is deactivated, not merely unreadable. */
    for (int i = 0; i < list.count; ++i)
        CHECK(strcmp(list.items[i].name, "vitaos_ps.suprx") != 0);
    CHECK(strcmp(list.items[0].name, "kubridge.skprx") == 0);
    CHECK(strcmp(list.items[1].name, "nonpdrm.skprx") == 0);
    CHECK(strcmp(list.items[2].name, "libshacccg.suprx") == 0);

    /* The full path is kept, because existence is checked against it. */
    CHECK(strcmp(list.items[0].path, "ur0:tai/kubridge.skprx") == 0);
    CHECK(strcmp(list.items[1].path, "ux0:tai/nonpdrm.skprx") == 0);

    /* Known plugins carry a description; unknown ones keep an empty one rather
     * than an invented one. Check every known row, not just the first: a
     * dropped table entry is silent at runtime and must fail here. */
    CHECK(list.items[0].desc[0] != '\0');
    CHECK(list.items[1].desc[0] != '\0');
    CHECK(list.items[2].desc[0] != '\0');

    PluginList unknown;
    memset(&unknown, 0, sizeof(unknown));
    plugins_parse("*main\nur0:tai/mystery.skprx\n", &unknown);
    CHECK(unknown.count == 1);
    CHECK(unknown.items[0].desc[0] == '\0');

    /* The files this console actually lists must resolve, not just the catalog
     * entries: a typo in a key leaves a real plugin with no description. */
    static const char *const NAMED[] = {"yamt.suprx", "henkaku.suprx",
                                        "vitabtfix.skprx",
                                        "storagemgr.skprx"};

    for (size_t i = 0; i < sizeof(NAMED) / sizeof(NAMED[0]); ++i) {
        char cfg[64];
        PluginList one;

        memset(&one, 0, sizeof(one));
        snprintf(cfg, sizeof(cfg), "*main\nur0:tai/%s\n", NAMED[i]);
        plugins_parse(cfg, &one);
        CHECK(one.count == 1);
        CHECK(one.items[0].desc[0] != '\0');
    }

    /* A header line is not a plugin, and an empty config yields nothing. */
    PluginList empty;
    memset(&empty, 0, sizeof(empty));
    plugins_parse("*KERNEL\n*main\n", &empty);
    CHECK(empty.count == 0);

    /* `*!Section` halts parsing; later paths are not reached. */
    PluginList halt;
    memset(&halt, 0, sizeof(halt));
    plugins_parse("*KERNEL\nur0:tai/a.skprx\n*!ALL\nur0:tai/b.skprx\n",
                  &halt);
    CHECK(halt.count == 1);
    CHECK(halt.config_invalid == 0);

    /* A path before the first section is a warning, not a lock: the later
     * valid entries are still parsed. */
    PluginList invalid;
    memset(&invalid, 0, sizeof(invalid));
    plugins_parse("ur0:tai/a.skprx\n*KERNEL\nur0:tai/b.skprx\n", &invalid);
    CHECK(invalid.config_invalid == 1);
    CHECK(invalid.count == 1);
    CHECK(section_of(&invalid, "b.skprx") == PLUGIN_SECTION_KERNEL);

    /* A BOM before the first header must not hide the section. */
    PluginList bom;
    memset(&bom, 0, sizeof(bom));
    plugins_parse("\xEF\xBB\xBF*KERNEL\nur0:tai/a.skprx\n", &bom);
    CHECK(bom.config_invalid == 0);
    CHECK(bom.count == 1);

    /* Suffixes are case-insensitive, as the Vita filesystem is. */
    PluginList upper;
    memset(&upper, 0, sizeof(upper));
    plugins_parse("*KERNEL\nur0:tai/A.SKPRX\n", &upper);
    CHECK(upper.count == 1);

    /* A prose comment ending in a plugin suffix is not a plugin line. */
    PluginList prose;
    memset(&prose, 0, sizeof(prose));
    plugins_parse("# see kubridge.skprx for the bridge\n*KERNEL\n"
                  "ur0:tai/kubridge.skprx\n", &prose);
    CHECK(prose.count == 1);
}

/* The section a named plugin was filed under, or -1 when it is absent. */
static int section_of(const PluginList *list, const char *name)
{
    for (int i = 0; i < list->count; ++i) {
        if (strcmp(list->items[i].name, name) == 0)
            return (int)list->items[i].section;
    }

    return -1;
}

/* Section headers decide the group. A file listed under several targets is
 * shown once, under the most privileged one; a path before any header is not
 * loaded and must not appear (D51). */
static void test_plugins_sections(void)
{
    static const char CFG[] = "ur0:tai/orphan.skprx\n"
                              "*NPXS10015\n"
                              "ur0:tai/settings.suprx\n"
                              "ur0:tai/shared.suprx\n"
                              "*KERNEL\n"
                              "ur0:tai/kern.skprx\n"
                              "ur0:tai/shared.suprx\n"
                              "*main\n"
                              "ur0:tai/shell.suprx\n"
                              "*NPXS10016\n"
                              "ur0:tai/widget.suprx\n"
                              "*ALL\n"
                              "ur0:tai/all.suprx\n"
                              "*PCSE00001\n"
                              "ur0:tai/game.suprx\n";

    PluginList list;
    memset(&list, 0, sizeof(list));
    plugins_parse(CFG, &list);

    CHECK(list.count == 7);
    CHECK(section_of(&list, "orphan.skprx") == -1);
    CHECK(section_of(&list, "settings.suprx") == PLUGIN_SECTION_SYSTEM_APP);
    CHECK(section_of(&list, "kern.skprx") == PLUGIN_SECTION_KERNEL);
    CHECK(section_of(&list, "shared.suprx") == PLUGIN_SECTION_KERNEL);
    CHECK(section_of(&list, "shell.suprx") == PLUGIN_SECTION_SYSTEM);
    CHECK(section_of(&list, "widget.suprx") == PLUGIN_SECTION_SYSTEM_APP);
    CHECK(section_of(&list, "all.suprx") == PLUGIN_SECTION_ALL_TITLES);
    CHECK(section_of(&list, "game.suprx") == PLUGIN_SECTION_TITLE);
}

/*
 * A real config lists more than a dozen plugins. The array used to hold 12, so
 * the tail of config.txt vanished without any sign; the ledger scrolls, so the
 * number of rows is a storage question, not a layout one.
 */
static void test_plugins_capacity(void)
{
    enum { N = 40 };
    char cfg[N * 32 + 16];
    size_t off = 0;

    off += (size_t)snprintf(cfg + off, sizeof(cfg) - off, "*KERNEL\n");

    for (int i = 0; i < N; ++i)
        off += (size_t)snprintf(cfg + off, sizeof(cfg) - off,
                                "ur0:tai/plugin%02d.skprx\n", i);

    PluginList list;
    memset(&list, 0, sizeof(list));

    int added = plugins_parse(cfg, &list);

    CHECK(added == N);
    CHECK(list.count == N);
    CHECK(list.truncated == 0);
    CHECK(strcmp(list.items[0].name, "plugin00.skprx") == 0);
    CHECK(strcmp(list.items[N - 1].name, "plugin39.skprx") == 0);
    CHECK(strcmp(list.items[N - 1].path, "ur0:tai/plugin39.skprx") == 0);
    CHECK(list.items[N - 1].section == PLUGIN_SECTION_KERNEL);
}

/* The longest catalog file name is 41 characters. A 32-byte field cut the name
 * and made its description lookup miss. */
static void test_plugins_long_name(void)
{
    static const char PATH[] =
        "ur0:tai/analogstickdisable_swapleftandright.skprx";
    char cfg[sizeof(PATH) + 8];
    PluginList list;

    memset(&list, 0, sizeof(list));
    snprintf(cfg, sizeof(cfg), "*main\n%s\n", PATH);
    plugins_parse(cfg, &list);

    CHECK(list.count == 1);
    CHECK(strcmp(list.items[0].name,
                 "analogstickdisable_swapleftandright.skprx") == 0);
    CHECK(strcmp(list.items[0].path, PATH) == 0);
    CHECK(list.items[0].desc[0] != '\0');
}

/* If the ceiling is ever reached the loss is recorded, so the panel can say so
 * rather than presenting a short list as complete. */
static void test_plugins_truncation(void)
{
    char cfg[(PLUGINS_MAX + 2) * 32];
    size_t off = 0;

    off += (size_t)snprintf(cfg + off, sizeof(cfg) - off, "*KERNEL\n");

    for (int i = 0; i < PLUGINS_MAX + 1; ++i)
        off += (size_t)snprintf(cfg + off, sizeof(cfg) - off,
                                "ur0:tai/plugin%02d.skprx\n", i);

    PluginList list;
    memset(&list, 0, sizeof(list));
    plugins_parse(cfg, &list);

    CHECK(list.count == PLUGINS_MAX);
    CHECK(list.truncated == 1);

    /* A repeated line after the ceiling loses nothing, so it must not raise the
     * flag: config.txt can list henkaku.suprx in several sections. */
    PluginList full;
    memset(&full, 0, sizeof(full));
    off = 0;

    off += (size_t)snprintf(cfg + off, sizeof(cfg) - off, "*KERNEL\n");

    for (int i = 0; i < PLUGINS_MAX; ++i)
        off += (size_t)snprintf(cfg + off, sizeof(cfg) - off,
                                "ur0:tai/plugin%02d.skprx\n", i);

    plugins_parse(cfg, &full);
    plugins_parse("ur0:tai/plugin00.skprx\n", &full);

    CHECK(full.count == PLUGINS_MAX);
    CHECK(full.truncated == 0);
}

static void test_plugins_scan(void)
{
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "ur0:tai/stray.skprx\n*KERNEL\nur0:tai/kubridge.skprx\n"
                  "#ur0:tai/0syscall6.skprx\nur0:tai/absent.skprx\n"
                  "*!ALL\nur0:tai/extra.skprx\n");
    plugins_scan();

    CHECK(g_plugins.config_found == 1);
    CHECK(g_plugins.config_invalid == 1);
    CHECK(g_plugins.count == 1);
    CHECK(strcmp(g_plugins.items[0].path, "ur0:tai/kubridge.skprx") == 0);

    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "\xEF\xBB\xBF*KERNEL\nur0:tai/kubridge.skprx\n");
    plugins_scan();
    CHECK(g_plugins.config_invalid == 0);
    CHECK(g_plugins.count == 1);

    fake_reset();
    fake_remove_file("ur0:tai/config.txt");
    plugins_scan();
    CHECK(g_plugins.config_found == 0);
    CHECK(g_plugins.count == 0);

    fake_reset();
    fake_add_file("ux0:tai/config.txt", "");
    plugins_scan();
    CHECK(g_plugins.config_found == 1);
    CHECK(g_plugins.count == 0);

    fake_reset();
    fake_fail_read(1);
    plugins_scan();
    CHECK(g_plugins.config_found == 0);
    CHECK(g_plugins.config_unreadable == 1);
    fake_fail_read(0);

    fake_reset();
    fake_add_file("ux0:tai/config.txt", "*main\nux0:tai/kubridge.skprx\n");
    fake_fail_read_path("ux0:tai/config.txt", 1);
    plugins_scan();
    CHECK(g_plugins.config_found == 0);
    CHECK(g_plugins.config_unreadable == 1);
    CHECK(g_plugins.count == 0);
    fake_fail_read_path("ux0:tai/config.txt", 0);
}

static void test_plugins_path_truncation(void)
{
    char path[128];
    char cfg[160];
    PluginList list;

    memset(path, 'a', sizeof(path) - 8);
    memcpy(path + sizeof(path) - 8, ".skprx", 7);
    path[sizeof(path) - 1] = '\0';

    snprintf(cfg, sizeof(cfg), "*KERNEL\n%s\n", path);
    memset(&list, 0, sizeof(list));
    plugins_parse(cfg, &list);

    CHECK(list.count == 0);
    CHECK(list.config_truncated == 1);
}

int main(void)
{
    test_alert_policy();
    test_cpu_usage();
    test_cpu_rates_absent();
    test_cpu_rates_frozen();
    test_cpu_rates_regress();
    test_cpu_rates_independent();
    test_cpu_active_mask();
    test_cpu_bad_unit_fails_closed();
    test_cpu_failed_read_leaves_trace_alone();
    test_storage_conversion();
    test_frame_history_needs_a_frame();
    test_power_field_validity();
    test_storage_error_is_carried();
    test_memory_conversion();
    test_history_ring();
    test_frame_floor();
    test_plugins_parse();
    test_plugins_sections();
    test_plugins_capacity();
    test_plugins_long_name();
    test_plugins_truncation();
    test_plugins_scan();
    test_plugins_path_truncation();

    printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
