#ifndef COCKPIT_UI_CORE_TEXT_H
#define COCKPIT_UI_CORE_TEXT_H

#include <stddef.h>

/*
 * CORE's readout strings.
 *
 * Formatting is pure so the "--" contract is testable on the host without a
 * panel, the renderer, or a device. The panel supplies both the value and its
 * validity; this module never decides whether a reading is real.
 */

/* "18%", or "--" when !valid. */
void core_text_format_core(char *buf, size_t n, int pct, int valid);

/*
 * Left: "CPU LOAD AVG 13%" or "--".
 * Right: "CTX 840/S  WAKE 230/S", with "--" for an unavailable rate.
 *
 * The two rates are independent: one counter can advance while the other is
 * frozen, and the caption must not turn that into a measured zero.
 */
void core_text_format_caption(char *left, size_t left_n,
                              char *right, size_t right_n,
                              int cpu_valid, int usage_pct,
                              int ctx_valid, int ctx_per_s,
                              int wake_valid, int wake_per_s);

#endif
