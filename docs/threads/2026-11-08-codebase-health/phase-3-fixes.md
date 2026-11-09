# Phase 3 — Fixes

Date: 2026-11-08
Branch: `review/codebase-health`
Baseline: `be928f6` (diagnosis checkpoints; build and 1,293 checks green)

The fixes were applied in logical batches with the full host suite and a Vita
build after each batch. The branch was kept uncommitted between batches (staged
mode); the final commit carries the whole result and this log.

## Batch 1 — Recovery and data safety

**Findings:** C1, C2, C3, C4, C5, C6, C7, C8
**Risk:** High (config/boot safety)
**RED:** New host tests for an orphan `.old`, an oversized `.undo`, a
comment-only `.undo`, no-live `.original`/`.session`, an unrecovered-write dock,
and a failed `.undo` snapshot. The fake gained a 64 KiB ceiling, a
recovery-failure hook, and a case-insensitive filesystem.
**GREEN:**
- `platform_write_config` recovers an orphan `.old` instead of deleting it,
  refuses a directory at either name, and only treats `ENOENT` as no live file.
- `editor_scan` recovers both mounts, records `recover_failed`, and probes all
  three snapshots on both mounts when no live config exists.
- An UNDO commit skips the stale check when the scan could not read or model the
  live file; a comment-only UNDO is committable because no-change compares bytes.
- A failed snapshot write probes/recovers the live `.undo` before rewriting the
  previous bytes.
- `plugin_flow_state` treats an unrecovered write as `LAST_WRITE` when UNDO is
  available; SAVE/CONFIRM are gated by `editor_readonly_reason()`.
**GREEN gate:** 1,360 host checks (at this batch), clean build.
**Files:** `src/core/plugin_editor.c/.h`, `src/platform/platform_psp2.c`,
`src/core/platform.h`, `src/ui/plugin_flow.c`, `tools/fake/*`, `tools/test/*`.

## Batch 2 — Model and UI correctness

**Findings:** C9, C10, C11, C12, C13, C14, C15, C24
**Risk:** Medium
**RED:** Tests for missing-file counts, section moves, KERNEL removal warnings,
battery-gated dock focus, zero-frame history, power field validity, and a
zero-buffer pad read (app-level; mock not linked).
**GREEN:**
- Missing files are no longer `ACTIVE`/`LOADED`.
- Preview work rows carry their section; a section move is `WILL MOVE` and a
  KERNEL move/removal raises the kernel warning.
- A commit restores selection by path, not index.
- `plugin_flow_revalidate_focus()` moves focus off an unavailable action.
- The pad read treats `<= 0` as failure and centres all axes; a failed sampling
  mode also centres.
- `hist_frame` accepts only real frame samples.
- POWER carries per-field validity; the panel renders `--` per field.
- Transient dock copy clears on tab/selection/dock changes.
**GREEN gate:** 1,371 checks, 38 previews, clean build.

## Batch 3 — Build identity and release integrity

**Findings:** W1, W2, W3, W4, W5, W6
**Risk:** Medium
**GREEN:**
- `SOURCE_HASH` covers `src`, `sce_sys`, `CMakeLists.txt`, `build.sh`, and the
  `SAFE` variant.
- `.source-stamp` lives beside `BUILD_NUMBER`, not in `build/`.
- The prune keeps exactly the expected VPK and removes `.vpk.out`,
  `_param.sfo`, old builds, and retired system-plugin/clock outputs.
- CMake rejects `COCKPIT_BUILD=0`.
- `send.sh` has no fixed-name fallback and prints the variant/basename.
**GREEN gate:** default b150 and safe b150 both build clean; switching variants
increments the number; `send.sh` cannot select the wrong name.

## Batch 4 — Smaller projects

**Findings:** A1–A7, K3–K6, K8, W7, W8
**Risk:** Medium
**GREEN:**
- cap-test uses the safe authid, a 5 s SceNet resolver, non-blocking connect with
  a 5 s select, 5 s send/recv timeouts, a paired `SceNet` unload, checked
  `vita2d`/PGF and `gethostbyname` results, and a named mkdir failure.
- kernel-demo is marked `blocked`; its app still cannot launch (K1) and the
  on-demand load is shell-gated (K2). The hook counter is atomic, the path copy
  is in an interrupt critical section, the NID docs are corrected, and the app
  checks `vita2d`/PGF.
- Both projects now have `docs/{PROJECT,ARCHITECTURE,DECISIONS}.md` and a
  packaging test.
**GREEN gate:** `cap-test` packaging test passes with the safe authid;
`kernel-demo` packaging test passes and prints the runtime block.

## Batch 5 — Copy, dead code, docs

**Findings:** C16–C23, C25–C28, T2, T3, W9
**Risk:** Low
**GREEN:**
- `plugins_scroll` has one writer through `plugin_nav_set_plugins_scroll`.
- `FOR DOCK` replaces the stale session-options prompt; `FREE USER MEM MB`
  matches the font.
- Choice-modal Left/Right input is gone.
- Negative error capacities do not reach `snprintf`; `stage_toggle` requires the
  row's exact next action.
- HOME says `PLUGINS LISTED`.
- The panel uses `editor_preview_diff`.
- Dead `hash_count`/`content_len`/`first_line`/`line_count` fields, suffix
  macros, and stale test fixtures are removed.
- Firmware copy is bounded to the 28-byte field.
- DESIGN/ARCHITECTURE formulas and allow-lists match the code; AGENTS/wiki paths
  are repository-relative.
**GREEN gate:** 1,371 checks, 38 previews, docs link check `none`, no whitespace
errors.

## Integration gate

- Default: `cockpit-v01.00-b150.vpk`, 130,853 bytes,
  SHA-256 `5143618f9affa68c275c1406819b32377c4662c3fccd143d0a9228a09f53555c`,
  VPK has `eboot.bin` + `sce_sys/` only.
- Safe: `cockpit-v01.00-b150-safe.vpk`, built out of tree, 0 warnings.
- Host: 130 metrics + 1,208 editor + 33 UI = 1,371 checks, 0 failures.
- Preview: 38 states, busiest 5,429/8,000.
- cap-test: safe packaging test OK.
- kernel-demo: packaging test OK, runtime blocked.
- Docs: AGENTS link/fence check `issues: none`; `git diff --check` clean.

## Deliberately deferred

- **K1/K2 redesign.** The user chose to mark kernel-demo blocked rather than
  redesign it. The next decision is retire vs file/shared-memory exchange vs
  boot plugin (safety rule 2).
- **Runtime tests for cap-test.** The app is device-only; the packaging test
  covers the safe authid and VPK layout.
- **On-console confirmation of the recovery paths.** The host fake models the
  adapter contract, not the Vita's `sceIoRename`/`sceIoGetstat` behavior.
