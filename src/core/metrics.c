#include "core/metrics.h"

#include <string.h>

/*
 * Sampling policy. This file contains no SDK call and no draw call: every
 * reading arrives through core/platform.h, which is what lets it be compiled
 * and exercised on a host against a fake platform (D7).
 *
 * The one writer of g_metrics (D16).
 */

#define BYTES_PER_MB (1024ull * 1024ull)

/* Ceiling on a reported capacity, so a misread reply cannot overflow the int
 * math in the panel's unit formatting. 1 TiB is far beyond any Vita volume. */
#define STORAGE_MB_MAX 1000000

#define FRAME_EMA_ALPHA 0.05f
#define FRAME_MS_FLOOR 0.1f
#define FRAME_MS_SEED 16.667f

Metrics g_metrics;

static float s_ema_ms = FRAME_MS_SEED;

/*
 * Worst frame since the last slow sample. The FRAME strip's axis is fixed at
 * two frame periods, and a sample reaching the top is meant to mean "a frame
 * was dropped" -- which the EMA can never show, because one 33 ms frame moves a
 * 16.7 ms baseline by under a pixel (review finding #12).
 */
static float s_peak_ms;

#define MICROS_PER_SECOND 1000000ull

/* A real console cannot switch threads or leave idle a million times a second.
 * The cap keeps a bad delta from overflowing the int the panel reads. */
#define CPU_RATE_MAX 1000000

/* The idle counter and the elapsed clock are read by separate calls. A core
 * that is fully idle can advance its idle clock a little past the wall reading;
 * allow 2 % (at least 2 ms) before declaring the sample unusable. A unit
 * finer than microseconds makes the delta larger and is rejected here; a
 * coarser unit makes it smaller, so the hardware checklist treats a static
 * 100 % at idle as unavailable. */
#define CPU_IDLE_SLACK_MIN_US 2000ull
#define CPU_IDLE_SLACK_DIV 50ull

/*
 * Previous CPU counters. The idle clocks are cumulative, so one window must
 * establish the baseline before a percentage can exist. This is policy state,
 * not platform state: the adapter returns raw counters only.
 */
static PlatformCpu s_prev_cpu;
static int s_have_prev_cpu;

static void sample_memory(void)
{
    PlatformMemory in;
    platform_read_memory(&in);

    g_metrics.mem_valid = in.ok;
    if (!in.ok)
        return;

    g_metrics.mem_user_mb = (int)(in.user_bytes / BYTES_PER_MB);
    g_metrics.mem_cdram_mb = (int)(in.cdram_bytes / BYTES_PER_MB);
    g_metrics.mem_phy_mb = (int)(in.phycont_bytes / BYTES_PER_MB);
}

/* Percent of one core that was *not* idle, from its idle-clock delta. */
static int cpu_pct_from_idle(uint64_t idle, uint64_t elapsed)
{
    if (elapsed == 0)
        return CPU_CORE_UNKNOWN;

    /*
     * The idle clock cannot have advanced further than wall time. A finer unit
     * makes the delta larger and is refused here; a coarser unit makes it
     * smaller and is not detectable from this comparison, so the hardware
     * checklist treats a static 100 % at idle as unavailable (see the [C]
     * note in platform_psp2.c). The small slack covers the two separate clock
     * reads, not a different unit.
     */
    if (idle > elapsed) {
        uint64_t slack = elapsed / CPU_IDLE_SLACK_DIV;

        if (slack < CPU_IDLE_SLACK_MIN_US)
            slack = CPU_IDLE_SLACK_MIN_US;
        if (idle > elapsed + slack)
            return CPU_CORE_UNKNOWN;

        idle = elapsed;
    }

    double busy = 100.0 - ((double)idle * 100.0 / (double)elapsed);
    int pct = (int)(busy + 0.5);

    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;

    return pct;
}

static int cpu_rate(uint64_t delta, uint64_t elapsed)
{
    uint64_t rate = (delta * MICROS_PER_SECOND + elapsed / 2) / elapsed;

    if (rate > CPU_RATE_MAX)
        rate = CPU_RATE_MAX;

    return (int)rate;
}

static void sample_cpu(void)
{
    PlatformCpu in;
    platform_read_cpu(&in);

    g_metrics.cpu_valid = 0;
    g_metrics.cpu_usage_pct = 0;
    g_metrics.cpu_ctx_per_s = 0;
    g_metrics.cpu_ctx_valid = 0;
    g_metrics.cpu_wake_per_s = 0;
    g_metrics.cpu_wake_valid = 0;

    for (int i = 0; i < CPU_CORE_COUNT; ++i)
        g_metrics.cpu_core_pct[i] = CPU_CORE_UNKNOWN;

    if (!in.ok) {
        s_have_prev_cpu = 0;
        return;
    }

    if (!s_have_prev_cpu || in.time_us <= s_prev_cpu.time_us) {
        s_prev_cpu = in;
        s_have_prev_cpu = 1;
        return;
    }

    uint64_t elapsed = in.time_us - s_prev_cpu.time_us;
    unsigned mask = in.active_mask & s_prev_cpu.active_mask;
    uint64_t idle_sum = 0;
    int cores = 0;

    for (int i = 0; i < CPU_CORE_COUNT; ++i) {
        if ((mask & (1u << i)) == 0)
            continue;
        if (in.idle_us[i] < s_prev_cpu.idle_us[i])
            continue;

        uint64_t idle = in.idle_us[i] - s_prev_cpu.idle_us[i];
        int pct = cpu_pct_from_idle(idle, elapsed);

        if (pct == CPU_CORE_UNKNOWN)
            continue;

        g_metrics.cpu_core_pct[i] = pct;
        idle_sum += idle;
        ++cores;
    }

    if (cores > 0) {
        double busy = 100.0 -
                      ((double)idle_sum * 100.0 /
                       ((double)elapsed * (double)cores));
        int pct = (int)(busy + 0.5);

        if (pct < 0)
            pct = 0;
        if (pct > 100)
            pct = 100;

        g_metrics.cpu_usage_pct = pct;
        g_metrics.cpu_valid = 1;
        history_push(&g_metrics.hist_cpu, (float)pct);
    }

    /*
     * A rate needs a strictly advancing total. Equal totals are a frozen or
     * zero-filled counter, not a measured 0/s, and a decreasing total is a
     * shrinking active mask or a 32-bit wrap. Both are unavailable.
     */
    if (in.ctx_switches > s_prev_cpu.ctx_switches) {
        g_metrics.cpu_ctx_per_s =
            cpu_rate(in.ctx_switches - s_prev_cpu.ctx_switches, elapsed);
        g_metrics.cpu_ctx_valid = 1;
    }
    if (in.wakeups > s_prev_cpu.wakeups) {
        g_metrics.cpu_wake_per_s =
            cpu_rate(in.wakeups - s_prev_cpu.wakeups, elapsed);
        g_metrics.cpu_wake_valid = 1;
    }

    s_prev_cpu = in;
}

static void sample_storage(int i)
{
    PlatformStorage in;
    platform_read_storage(i, &in);

    g_metrics.storage[i].error = in.error;
    g_metrics.storage[i].errno_byte = in.errno_byte;
    g_metrics.storage[i].valid = in.ok;
    if (!in.ok)
        return;

    uint64_t total_mb = in.total_bytes / BYTES_PER_MB;
    uint64_t used_mb = (in.total_bytes - in.free_bytes) / BYTES_PER_MB;

    if (total_mb > STORAGE_MB_MAX)
        total_mb = STORAGE_MB_MAX;
    if (used_mb > total_mb)
        used_mb = total_mb;

    g_metrics.storage[i].total_mb = (int)total_mb;
    g_metrics.storage[i].used_mb = (int)used_mb;
}

static void sample_power(void)
{
    PlatformPower in;
    platform_read_power(&in);

    g_metrics.power_valid = in.ok;
    g_metrics.batt_temp_valid = in.temp_ok;
    g_metrics.batt_mv_valid = in.volt_ok;
    g_metrics.batt_cycles_valid = in.cycles_ok;
    g_metrics.batt_health_valid = in.health_ok;

    if (!in.ok)
        return;

    g_metrics.batt_pct = in.percent;
    g_metrics.batt_temp_c = in.temp_c;
    g_metrics.batt_mv = in.millivolts;
    g_metrics.batt_cycles = in.cycles;
    g_metrics.batt_health = in.health;
    g_metrics.charging = in.charging;
}

static void sample_identity(void)
{
    PlatformIdentity in;
    platform_read_identity(&in);

    g_metrics.model = in.model;
    g_metrics.fw_valid = in.fw_ok;

    if (in.fw_ok) {
        memcpy(g_metrics.fw, in.fw, FW_STRING_MAX);
        g_metrics.fw[FW_STRING_MAX - 1] = '\0';
    }
}

static void sample_clock(void)
{
    PlatformClock in;
    platform_read_clock(&in);

    g_metrics.clock_valid = in.ok;
    if (!in.ok)
        return;

    g_metrics.clock_h = in.hour;
    g_metrics.clock_m = in.minute;
    g_metrics.clock_s = in.second;
}

static void update_alert(void)
{
    int a = 0;

    if (!g_metrics.mem_valid)
        a = 1;

    /*
     * A failed power read is its own fault. Without this the band would clear
     * the moment scePowerGetBatteryLifePercent() started returning a negative
     * value, silently dropping an alert that was already true (D23).
     */
    if (!g_metrics.power_valid)
        a = 1;

    if (g_metrics.power_valid && g_metrics.batt_pct < LOW_BATTERY_PCT &&
        !g_metrics.charging)
        a = 1;

    if (!g_metrics.storage[PLATFORM_STORAGE_UX0].valid)
        a = 1;

    g_metrics.alert = a;
}

void metrics_init(void)
{
    memset(&g_metrics, 0, sizeof(g_metrics));
    history_reset(&g_metrics.hist_cpu);
    history_reset(&g_metrics.hist_frame);

    s_ema_ms = FRAME_MS_SEED;
    s_peak_ms = 0.0f;
    s_have_prev_cpu = 0;
    g_metrics.frame_ms = s_ema_ms;
    g_metrics.fps = 1000.0f / s_ema_ms;

    metrics_sample_slow();
}

void metrics_sample_slow(void)
{
    sample_memory();

    for (int i = 0; i < STORAGE_COUNT; ++i)
        sample_storage(i);

    sample_power();
    sample_identity();
    sample_clock();
    sample_cpu();

    g_metrics.uptime_s = platform_uptime_seconds();

    /*
     * A slow sample can run before any frame, or after a catch-up burst that
     * already consumed the peak. Only a real frame sample belongs in the
     * trace; a zero would draw a false dropped-frame dip.
     */
    if (s_peak_ms > 0.0f)
        history_push(&g_metrics.hist_frame, s_peak_ms);
    s_peak_ms = 0.0f;

    update_alert();
}

void metrics_sample_frame(float dt)
{
    float ms = dt * 1000.0f;
    if (ms < 0.0f)
        ms = 0.0f;

    s_ema_ms += (ms - s_ema_ms) * FRAME_EMA_ALPHA;
    if (s_ema_ms < FRAME_MS_FLOOR)
        s_ema_ms = FRAME_MS_FLOOR;

    g_metrics.frame_ms = s_ema_ms;
    g_metrics.fps = 1000.0f / s_ema_ms;

    if (ms > s_peak_ms)
        s_peak_ms = ms;
}
