## Code Review Results

**Scope:** `HEAD^` (`a4ef5d4`) -> working tree at `1910e0f` (24 files, 908 changed lines, 497 executable non-test)
**Intent:** Replace CORE's memory-only panel with real userland CPU activity from `sceKernelGetSystemInfo`: per-core load, active-core average, context-switch/wakeup rates, and a fixed 0-100 % trace. Keep the memory pools; add no kernel plugin and no fabricated GPU load or applied clocks. Must fail closed to `--` when the scheduler counters are unavailable or the idle-clock unit is wrong.
**Mode:** markdown, report-only (no local apply)

**Reviewers:** correctness, project-standards, testing, maintainability, adversarial
- correctness -- always-on
- project-standards -- the root `AGENTS.md` governs every changed file
- testing -- new tests and changed runtime sampling behavior
- maintainability -- 497 executable non-test changed lines and a new port surface
- adversarial -- large executable diff; the cross-model peer could not run, so an in-process reviewer covered that lens

### Triage Groups

| Group | Findings | Context | Preferred Resolution | Why |
|-------|----------|---------|----------------------|-----|
| G1 -- Fail-closed CPU readout (decision-gate) | #1, #2 | Both leave one honesty failure: a value the platform cannot support is drawn as a measurement. #1 leaves one direction of an idle-clock unit error undetected while four docs promise it is caught; #2 draws `CTX 0/S WAKE 0/S` when the cumulative counters are absent or regress. | Decide #1 first -- correct the docs to the one-sided guarantee or add the opposite-direction guard -- then implement #2's per-rate validity flags so the caption follows the same `--` contract. | #1's decision fixes the wording and hardware checklist that #2's tests and docs will follow. |
| G2 -- Verification claims vs. actual coverage (apply-queue) | #3, #7 | #3: no test or preview exercises the panel's `--` branch. #7: the `[V]` ledger marks the fixed 0-100 % trace host-test verified although no test observes the axis. | Do #3 first (preview scene plus a pure formatting helper the harness links), then correct #7's rows to what is actually asserted. | Both let the docs claim verification the suite cannot produce; added coverage shrinks what #7 has to reword. |
| G3 -- Docs and comments that lag the shipped behavior (apply-queue) | #4, #6, #8 | `ARCHITECTURE.md:320` names the wrong file for the unit guard, `metrics.h:98`'s contract comment omits CPU, and `PROJECTS.md:26`'s Next cell is a three-action checklist. | Mechanical edits in any order: add `core/metrics.c` to the `[C]` row's Where column, add CPU to the header comment, and reduce the Next cell to the flash-and-confirm step. | Three independent documentation inaccuracies introduced by the same diff; one pass over the touched docs closes them. |

Split for the actor: #1 is the only design call. #2-#8 are mechanical edits and tests. #5 is ungrouped cleanup.

### P1 -- High

| # | File | Issue | Reviewer | Confidence |
|---|------|-------|----------|------------|
| 1 | `vita-cockpit/src/core/metrics.c:82` | Idle-clock unit check is one-sided; docs promise fail-closed | project-standards, correctness | 75 |

- **#1** -- `cpu_pct_from_idle()` rejects an idle delta larger than elapsed time, which catches only a counter that advances *faster* than microseconds. A coarser unit (milliseconds, seconds, a tick above 1 us) makes every idle delta a small fraction of elapsed, so the ratio rounds to a permanent ~100 % and `cpu_valid` stays set. That is the exact fabricated hero number D6/D103 forbid and the static-100 % symptom the hardware checklist says must never appear, indistinguishable from a genuinely saturated machine. Four docs and comments promise a full guarantee the arithmetic does not provide: `platform.h:83-85`, `platform_psp2.c:165-166`, `DECISIONS.md:1997` (D103), `ARCHITECTURE.md:320`, `PROJECTS.md:26`, plus the comment at `metrics.c:77-88`.
  **Response -- design call, pick one:** (a) minimal: correct the guarantee everywhere to say only a counter faster than wall time is rejected, and add to `PROJECT.md`'s hardware checklist "a static ~100 % at idle means `idleClock` is coarser than microseconds; record it and treat CPU as unavailable"; or (b) behavioral: add the opposite-direction guard -- if every active core reads exactly 100 % for five consecutive windows (0.5 s), refuse the sample and render `--` until the reading drops. (a) is the cheap, defensible default; (b) is the only one that makes the promise true. Validator confirmed the arithmetic and doc mismatch; hardware incidence was not measured, so this is a defect the code permits that no one has shown to occur. The two reviewers split P1/P2; the cautious rating stands because a wrong unit fabricates the hero number.

### P2 -- Moderate

| # | File | Issue | Reviewer | Confidence |
|---|------|-------|----------|------------|
| 2 | `vita-cockpit/src/core/metrics.c:178` | Fabricated `CTX 0/S  WAKE 0/S` when the cumulative counters are absent or regress | maintainability, project-standards, adversarial | 100 |
| 3 | `vita-cockpit/src/panels/core.c:61` | No test or preview renders the CPU fail-closed `--` path | testing | 75 |

- **#2** -- `sample_cpu()` zeroes both rates at the top of every sample, then skips the assignment when a cumulative counter has not advanced or has gone backwards; `cpu_valid` is set from the idle clocks alone, so the caption's only gate stays open and `CTX 0/S  WAKE 0/S` prints as measured. The adapter sums the counters over the cores active at read time, so a narrowing `activeCpuMask` or a 32-bit `threadSwitchCount`/`comesOutOfIdleCount` wrap makes the total fall. Zero switches per second is impossible on a running Vita. **Fix:** add `int cpu_ctx_valid; int cpu_wake_valid;` to the CPU block in `Metrics`, clear them with the other CPU fields in `sample_cpu()`, set each only inside its `>=` branch, and have `draw_caption()` print `--` per field -- the pattern the POWER panel already uses. Add zero, unchanged, and backwards-counter cases to `tools/test/test_metrics.c`, and pin the shrinking-mask case.
- **#3** -- `tools/test/run.sh:20-25` links no `panels/` source, so `draw_cpu_row` and `draw_caption` cannot be covered; the only `fake_set_cpu_absent()` call asserts `g_metrics` fields, and no preview scene renders the `THEME_ABSENT` branch. If the fallback regressed, every host check would pass and the first evidence would come from the console run. **Fix:** add a preview scene that calls `fake_set_cpu_absent()` (and a second with `fake_set_cpu_mask(0x5)`) before `metrics_sample_slow()`, mirroring the uma0-absent default that exercises STORAGE's `--`; for an automated gate, extract the per-core and caption formatting into a pure helper the test harness links and assert it emits `--` for `CPU_CORE_UNKNOWN` and `cpu_valid == 0`.

### P3 -- Low

| # | File | Issue | Reviewer | Confidence |
|---|------|-------|----------|------------|
| 4 | `vita-cockpit/docs/ARCHITECTURE.md:320` | `[C]` row places the unit rejection in the adapter, not the policy | project-standards | 100 |
| 5 | `vita-cockpit/src/core/metrics.h:56` | Added CPU validity mask and CPU rate test hook have no consumer | correctness, maintainability, project-standards | 100 |
| 6 | `vita-cockpit/src/core/metrics.h:98` | `metrics_sample_slow()` contract comment omits CPU | maintainability | 100 |
| 7 | `agent-wiki/13-fact-check.md:88` | Docs claim `[V]` coverage of a trace axis no test asserts | testing | 75 |
| 8 | `PROJECTS.md:26` | Next cell lists three actions, not one | project-standards | 75 |

- **#4** -- The row says "the adapter rejects a delta larger than elapsed time" and names `platform_psp2.c`, which contains no such check; the rejection is `cpu_pct_from_idle()` in `core/metrics.c`. `ARCHITECTURE.md:333` promises a hardware correction lands in the file its Where column names, so this row sends a maintainer to the wrong file. **Fix:** reword the Evidence cell and list `core/metrics.c` in the Where column beside `platform_psp2.c`.
- **#5** -- `g_metrics.cpu_active_mask` is written twice and read nowhere, and it records the pre-filter candidate mask rather than the cores that produced a percentage -- a later consumer would treat unmeasured cores as measured. `fake_set_cpu_activity()` is declared and defined but never called, while the rate assertions silently rely on `cpu_reset()`'s 840/230 defaults. **Fix:** delete the mask field and its two writes, and either delete the fake setter or make it the tested source (`fake_set_cpu_activity(900, 250)` with assertions on 900/250). Unused-symbol evidence is text-search only; that boundary is recorded under residual risks.
- **#6** -- The header says a slow sample reads "Memory, storage, power, identity, wall clock", but this diff made CPU the sixth and hero reading, and `docs/ARCHITECTURE.md:270` already lists CPU in the 10 Hz cadence. **Fix:** change the comment to `/* ~10 Hz. CPU, memory, storage, power, identity, wall clock. */`.
- **#7** -- The only `hist_cpu` assertions are ring length and newest value; the `0.0f`/`100.0f` axis literals live in `panels/core.c:115-116`, which the test harness never links, and no preview scene asserts an axis. A future autoscale change would leave every check green while the `[V]` claim still reads verified. The same claim is repeated at `DECISIONS.md:2000` and `PROJECTS.md:44`. **Fix:** record the sparkline's lo/hi arguments in the preview shim and assert 0/100, or reword the three claims to "host tests cover the trace data; the preview renders it on the fixed 0-100 % axis".
- **#8** -- The Next cell chains flash, CPU confirmation, and the remaining dashboard/manager checklist. The board rule is one action. **Fix:** reduce it to "Flash `cockpit-v01.00-b153.vpk` and confirm CPU load moves and is not a static 100 %/`--`" and move the rest of the checklist to Notes with a pointer to `vita-cockpit/docs/PROJECT.md` section 5.

### Actionable Findings

| # | File | Issue | Route | Notes |
|---|------|-------|-------|-------|
| 1 | `vita-cockpit/src/core/metrics.c:82` | Idle-clock unit guard is one-sided but documented as fail-closed | `manual -> downstream-resolver` | `suggested_fix` present; design call between docs correction (default) and opposite-direction guard |
| 2 | `vita-cockpit/src/core/metrics.c:178` | Fabricated `CTX 0/S  WAKE 0/S` when counters are absent or regress | `gated_auto -> downstream-resolver` | `suggested_fix` present; per-field validity flags, POWER pattern |
| 3 | `vita-cockpit/src/panels/core.c:61` | No test or preview renders the CPU fail-closed `--` path | `gated_auto -> downstream-resolver` | `suggested_fix` present; preview scene plus pure formatting helper |
| 4 | `vita-cockpit/docs/ARCHITECTURE.md:320` | `[C]` row names the adapter for a policy guard | `gated_auto -> downstream-resolver` | `suggested_fix` present |
| 5 | `vita-cockpit/src/core/metrics.h:56` | Unused `cpu_active_mask` and `fake_set_cpu_activity()` | `gated_auto -> downstream-resolver` | `suggested_fix` present; ~9 removable lines |
| 6 | `vita-cockpit/src/core/metrics.h:98` | Slow-sample contract comment omits CPU | `gated_auto -> downstream-resolver` | `suggested_fix` present |
| 7 | `agent-wiki/13-fact-check.md:88` | `[V]` claim for a trace axis no test asserts | `gated_auto -> downstream-resolver` | `suggested_fix` present; also `DECISIONS.md:2000`, `PROJECTS.md:44` |
| 8 | `PROJECTS.md:26` | Next cell is a three-action checklist | `gated_auto -> downstream-resolver` | `suggested_fix` present |

### Pre-existing Issues

| # | File | Issue | Reviewer |
|---|------|-------|----------|
| PE-1 | `agent-wiki/examples/cockpit/platform.h:54` | Example copies still show the clock-era port: `PlatformClocks`, three storage mounts, no `PlatformCpu`/`platform_read_cpu` | project-standards |

`agent-wiki/12-projects.md:16` advertises `examples/cockpit/` as the reference for "the port, the adapter", but those copies predate D99, so the wiki teaches the opposite of the shipped port. Provenance: `5397df1 Import the PS Vita homebrew workspace`, the only commit touching these copies. Regenerate both example files from `vita-cockpit/src/core/platform.h` and `platform_psp2.c`. Informational; does not count toward the verdict. Pre-existing identifiers are separate from the primary set.

### Coverage

- **Depth:** full. 497 executable non-test changed lines; the scope helper reported `size_band=large` and a 301-line unclassified floor, which set the full-spine floor.
- **Collection:** 5 reviewers ran, 0 failed, 0 bound-exceeded; 17 raw findings, 0 malformed findings or returns; 16 `first_evidence` values backfilled, 0 quote-check demotions.
- **Suppressed:** 1 finding at anchor 50, 0 at anchor 25. The anchor-50 candidate (correctness, wrong-unit guard) was reconciled into #1 as the same defect and fix path, not discarded.
- **Soft-bucket demotions:** 2. A testing P2 on rate tests being unable to tell per-tick from per-second moved to testing gaps; a maintainability P3 on duplicated percent rounding moved to residual risks.
- **Validator:** one batch, 8 selected (all primary findings), 0 shortcut skips -- no `adversarial-<provider>` peer artifact exists and no return records `independence_verified: true`, so no finding met the cross-model shortcut. Verdicts landed inside the wait bound: 8 confirmed, 0 rejected, 0 unresolved, 0 malformed. None of the eight alleges a protected subject (memory safety, concurrency, data loss, authorization, injection, public contract, secrets, crypto), so no protected-subject reclassification was needed and no rejection required a cited refutation. #1 and #2 were confirmed with incidence not measured and carry `validation_status: confirmed` plus that reason: real defects the code permits, with no hardware observation yet.
- **Peer:** cross-model pass: not run; host serving family is un-attestable under OpenCode/DeepSeek, so the in-process adversarial reviewer covered the lens.
- **Standards:** project standards ran against the root `AGENTS.md` instruction-file fallback; no `CODING_STANDARDS.md` exists in the tree.
- **Settlement:** settlement suppression was not evaluated (no plan discovered in Stage 2b).
- **Excluded:** untracked `vitaFTPShell/` was not reviewed.
- **Testing gaps:**
  - Rate tests cannot distinguish per-tick from per-second: every fake read advances exactly 100 ms, so a `delta * 10` body still passes 145 checks. A configurable fake step plus a 500 ms case would pin it, and would give `fake_set_cpu_activity` a caller.
  - Counter-reset guards at `metrics.c:147` and `:178`/`:181` are untested; the fake has no setter for a decreasing counter, so deleting either guard leaves the suite green.
  - `cpu_pct_from_idle`'s slack path is unexercised: only `idle = 2 x elapsed` is tested, not the accept-and-clamp case where a fully idle core must render 0 %, not `--`. The unclamped delta also feeds `idle_sum`, where the aggregate mismatch comes from.
  - The baseline-only state after `metrics_init()` is not asserted: `cpu_valid` must be 0, every core `CPU_CORE_UNKNOWN`, and `hist_cpu` empty until a second window exists.
  - Recovery after a failed or rejected CPU sample is untested; `test_cpu_failed_read_leaves_trace_alone` stops at the failure, so the two-window blank that follows is unpinned.
  - No test narrows `activeCpuMask` between samples or wraps a 32-bit `threadSwitchCount`, so the #2 regression path and the mask intersection are unverified.
  - All four active cores at 100 % (every idle delta zero) must yield a valid 100 average; only a single 100 % core is covered.
  - The aggregate's +0.5 rounding is unpinned: the fixture mean 13.25 truncates to the same 13, so plain truncation would still pass; a 13.5 mean would distinguish them.
  - The diff removed `test_failed_memory_leaves_trace_alone`; `fake_set_memory_absent()` now only feeds `test_alert_policy`, so nothing asserts `mem_valid == 0` or that the three pools render `--` after a failed memory read.
- **Residual risks:**
  - Catch-up sampling: `app.c:501-506` re-runs `metrics_sample_slow()` up to 3 times in a tight loop, so sub-window elapsed can read 100 %, a single switch can become a five-figure CTX rate, and the sample lands in a trace whose axis assumes 100 ms windows. The fixed 100 ms fake cannot reproduce it.
  - The aggregate averages the raw idle delta while `cpu_pct_from_idle` clamps its copy, so in the slack band the average can read up to ~0.5 points per fully idle core below the displayed cores. Bounded and cosmetic; the duplicated rounding at `metrics.c:94-99` and `:165-170` is where the two paths drift.
  - No upper bound on elapsed: a multi-second gap (standby, debugger pause, long stall) is averaged as one 100 ms window; the first post-resume sample is a fabricated ~100 % or a rejected blank.
  - `PLATFORM_CPU_COUNT` (4) mirrors the SDK's `cpuInfo[4]` with no static assertion; a future bump to the port constant would read past the struct. A `_Static_assert` in the adapter would pin it.
  - `sceKernelGetSystemInfo` is a strong `SceLibKernel` import, so a missing NID on 3.65 fails at module load instead of degrading to `--`; fail-closed policy cannot cover an unresolved import.
  - `agent-wiki/04-userland-apps.md` "Power and clocks" was not updated with the `sceKernelGetSystemInfo` CPU-load route.
  - The `[C]` idle-clock unit and the adapter's four refusal paths can only be settled on the console; the fake emits microsecond deltas by construction.
  - The unused-symbol claims behind #5 rest on text search only (`callsite completeness: grep-only`); a macro-generated or build-time reference would not appear. No out-of-tree consumer exists in the workspace.
- **Removable surface:** ~9 lines across 4 files from #5 (`metrics.h`, `metrics.c`, `fake_platform.h`, `fake_platform.c`). Dead-weight signal only, not a reduction target.
- **Inspection boundary:** the testing reviewer reported the host suite run (145 metrics, 1,208 editor, 33 UI checks); the preview renderer and the PNG-level appearance of the new CPU rows were not exercised, and no runtime path has run on the console.
- **Run artifacts:** `/tmp/compound-engineering-501/ce-code-review/20261108-105324-38da7151/` -- `metadata.json` written, `report.md` persisted by the dispatcher from this return.

---

> **Verdict:** Ready with fixes
>
> **Reasoning:** No P0. One confirmed P1 (#1) leaves the fail-closed CPU readout one-sided: a coarser `idleClock` unit passes the only check and renders a permanent ~100 %, and no one has measured that on this console yet. Two P2s follow from the same honesty contract (#2) and from missing coverage of the `--` path (#3). Five P3s are documentation, comment, tracker, and dead-code cleanup. The intended behavior is sound; the guarantee around it is not yet true, and the verdict cannot rise above "Ready with fixes" while #1 is open.
>
> **Fix order:** settle #1 (docs correction or opposite-direction guard) -> #2 per-rate validity flags -> #3 preview/test coverage -> #4, #6, #7, #8 doc/comment/tracker edits -> #5 dead-code removal.

### Actionable Recap

**Verdict: Ready with fixes.** The one thing to settle first is #1: the idle-clock unit guard catches only a counter faster than wall time, so a coarser unit can still draw a permanent ~100 %. Report-only run; no fixes were applied.

1. **P1 #1** -- `vita-cockpit/src/core/metrics.c:82` -- idle-clock unit check is one-sided while five docs promise fail-closed. Design call: correct the guarantee (default) or add the opposite-direction guard. `manual -> downstream-resolver`, confidence 75, `suggested_fix` present.
2. **P2 #2** -- `vita-cockpit/src/core/metrics.c:178` -- `CTX 0/S WAKE 0/S` is drawn when the cumulative counters are absent or regress. Add per-field validity flags and render `--`. `gated_auto -> downstream-resolver`, confidence 100, `suggested_fix` present.
3. **P2 #3** -- `vita-cockpit/src/panels/core.c:61` -- no test or preview renders the CPU `--` branch. Add a preview scene and a pure formatting helper the harness links. `gated_auto -> downstream-resolver`, confidence 75, `suggested_fix` present.
4. **P3 #4** -- `vita-cockpit/docs/ARCHITECTURE.md:320` -- `[C]` row names the adapter for a guard that lives in `core/metrics.c`. `gated_auto -> downstream-resolver`, confidence 100, `suggested_fix` present.
5. **P3 #5** -- `vita-cockpit/src/core/metrics.h:56` -- `cpu_active_mask` is never read and `fake_set_cpu_activity()` is never called. `gated_auto -> downstream-resolver`, confidence 100, `suggested_fix` present.
6. **P3 #6** -- `vita-cockpit/src/core/metrics.h:98` -- slow-sample contract comment omits CPU. `gated_auto -> downstream-resolver`, confidence 100, `suggested_fix` present.
7. **P3 #7** -- `agent-wiki/13-fact-check.md:88` -- `[V]` claim for a trace axis no test asserts. `gated_auto -> downstream-resolver`, confidence 75, `suggested_fix` present.
8. **P3 #8** -- `PROJECTS.md:26` -- Next cell is a three-action checklist. `gated_auto -> downstream-resolver`, confidence 75, `suggested_fix` present.

**Not in the queue:** PE-1, pre-existing drift in `agent-wiki/examples/cockpit/` (informational, does not affect the verdict).

**Run artifacts:** `/tmp/compound-engineering-501/ce-code-review/20261108-105324-38da7151/` (`metadata.json`; `report.md` persisted by the dispatcher).
