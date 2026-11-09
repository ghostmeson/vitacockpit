#ifndef COCKPIT_CORE_METRICS_H
#define COCKPIT_CORE_METRICS_H

#include "core/history.h"
#include "core/platform.h"

#define STORAGE_COUNT PLATFORM_STORAGE_COUNT
#define FW_STRING_MAX PLATFORM_FW_MAX

/* Alert threshold, percent, discharging only. */
#define LOW_BATTERY_PCT 20

#define CPU_CORE_COUNT PLATFORM_CPU_COUNT

/* A core that is not active has no percentage to report. */
#define CPU_CORE_UNKNOWN (-1)

/*
 * Storage in whole megabytes.
 *
 * MB rather than tenths of a GB so a small volume keeps its precision: a 512 MB
 * card is "512 MB", not "0.5 GB". The panel picks the unit from the magnitude.
 */
typedef struct {
    int used_mb;
    int total_mb;
    int error; /* the platform call's return code, 0 when it succeeded */
    unsigned errno_byte; /* low byte of `error`, already decoded by the port */
    int valid;
} StorageSample;

/*
 * One snapshot of the machine, already converted to the units the HUD speaks.
 *
 * Unit conversion happens here, not in a panel, so that "what a gigabyte means"
 * has one home. Panels format strings; they do not decide magnitudes.
 *
 * Every field has a matching `*_valid` flag. A panel must render "--" when the
 * flag is clear rather than printing the zero it happens to hold.
 */
typedef struct {
    int mem_user_mb;
    int mem_cdram_mb;
    int mem_phy_mb;
    int mem_valid;

    /*
     * CPU load is system-wide: it comes from the scheduler's per-core idle
     * clocks, not from this process. `cpu_core_pct` is CPU_CORE_UNKNOWN for a
     * core outside the active mask. `cpu_valid` means at least one core had a
     * usable idle delta.
     */
    int cpu_valid;
    int cpu_usage_pct; /* active-core average */
    int cpu_core_pct[CPU_CORE_COUNT];

    /*
     * The cumulative scheduler counters are summed over the active cores. A
     * total that does not advance (a frozen or zero-filled field) or goes
     * backwards (a shrinking active mask or a 32-bit wrap) cannot produce a
     * rate, so each rate has its own validity flag. A panel renders "--" for
     * the invalid one; it never prints the cleared zero as a measurement.
     */
    int cpu_ctx_per_s;
    int cpu_ctx_valid;
    int cpu_wake_per_s;
    int cpu_wake_valid;

    StorageSample storage[STORAGE_COUNT]; /* 0 ux0, 1 uma0 */

    int batt_pct;
    int batt_temp_c;
    int batt_mv;
    int batt_cycles;
    int batt_health;
    int charging;
    int power_valid;
    int batt_temp_valid;
    int batt_mv_valid;
    int batt_cycles_valid;
    int batt_health_valid;

    PlatformModel model;
    char fw[FW_STRING_MAX];
    int fw_valid;

    double uptime_s;
    float fps;
    float frame_ms;

    int clock_h;
    int clock_m;
    int clock_s;
    int clock_valid;

    History hist_cpu;   /* aggregate CPU load, percent */
    History hist_frame; /* frame time, ms */

    /* Drives the hazard band. Never true on a healthy console. */
    int alert;
} Metrics;

extern Metrics g_metrics;

void metrics_init(void);

/* ~10 Hz. CPU, memory, storage, power, identity, wall clock. */
void metrics_sample_slow(void);

/* Every frame. Frame time and FPS only. */
void metrics_sample_frame(float dt);

#endif
