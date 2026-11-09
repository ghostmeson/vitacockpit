# Phase 2 — Diagnosis

Date: 2026-11-08
Scope: whole workspace. Findings are ordered by severity and grouped by the
component that must change. Every finding was checked against the current source;
line numbers are from this checkout.

## 1. Health scores

| Area | Score | Key issue |
|---|---|---|
| COCKPIT architecture / dependency rule | 9/10 | clean port, one SDK adapter, one writer for the main models |
| COCKPIT correctness | 6/10 | several recovery and UI-state defects; no memory-safety bug found |
| COCKPIT recovery / data safety | 4/10 | `.old` can be deleted; two UNDO dead ends; unverified overwrite path |
| COCKPIT tests | 7/10 | 1,293 checks, but the fake cannot reach the most dangerous branches |
| COCKPIT build / release identity | 5/10 | safe/default share a number; assets not hashed; stale artifacts |
| COCKPIT docs | 7/10 | accurate after D99, but two layout/allow-list inconsistencies remain |
| `vita-cap-test` correctness | 5/10 | blocking network sequence before the UI loop; unchecked teardown |
| `vita-cap-test` tests / docs | 1/10 | no tests, no `docs/` tree |
| `vita-kernel-demo` correctness | 1/10 | app cannot launch (strong self-import); on-demand load is shell-gated |
| `vita-kernel-demo` tests / docs | 1/10 | no tests, no `docs/` tree, wrong NID documented |
| Workspace build / artifact rules | 5/10 | variants share numbers; "one artifact" is not true on disk |
| **Overall** | **5.4/10** | good architecture, weak recovery and release integrity |

## 2. Findings

### CRITICAL

| ID | Finding | File:line | Effort | Unlocks |
|---|---|---|---|---|
| K1 | The kernel-demo app links the **strong** stub of the plugin it loads at runtime, so the loader must resolve an import from a module that does not exist yet; `main()` cannot run. | `vita-kernel-demo/CMakeLists.txt:87`, `app/main.c:11-12,41` | Moderate | K2 decision |
| K2 | The on-demand kernel load is shell-gated on this console: the app's `taiLoadStartKernelModule` path is denied to a plain app, so even after K1 the demo cannot work as designed. | `vita-kernel-demo/app/main.c:41`; wiki `06`/`04` conflict | Significant | K8, W8 |

### HIGH

| ID | Finding | File:line | Effort | Unlocks |
|---|---|---|---|---|
| C1 | `platform_write_config` deletes `<path>.old` before checking whether the live file exists. After an interrupted swap, `.old` can be the only copy; a later failed write can destroy it. | `src/platform/platform_psp2.c:365-375,414` | Moderate | C7 |
| C2 | An oversized live config sets `source_ok=1` with an empty baseline, advertises `.undo`, then refuses every UNDO with `CONFIG CHANGED ON DISK`; restarting cannot help. | `src/core/plugin_editor.c:844-886,1352-1363,1818-1827` | Quick | C3 |
| C3 | After an unrecovered write the dock still shows SAVE/CONFIRM as available and the primary action; both refuse with `REPAIR WITH UNDO FIRST`, and UNDO is not in the draft action list. Rows also remain editable. | `src/ui/plugin_flow.c:19-37,67-73,268-279`; `src/core/plugin_editor.c:988-990,1407-1408` | Moderate | C4 |
| A1 | `vita-cap-test` runs blocking `gethostbyname`, `connect`, `send`, and `recv` before the draw/input loop, with no socket timeouts; a bad network leaves the screen on "Running tests..." with no exit. | `vita-cap-test/src/main.c:203,224,237,245,325-337` | Moderate | A8 |
| W1 | Safe and default builds share one build number because `--safe` is parsed after the hash; building safe last leaves only the safe VPK and `send.sh` uploads it without warning. | `vita-cockpit/build.sh:35-50,56-77`; `send.sh:8` | Quick | W2, W4 |

### MEDIUM

| ID | Finding | File:line | Effort |
|---|---|---|---|
| C4 | With no live config, ORIGINAL/SESSION recovery is unreachable; only `.undo` is probed, so a deleted config with a valid snapshot offers nothing. | `plugin_editor.c:790-792,866-882,1607-1611` | Moderate |
| C5 | A failed `.undo` snapshot can lose the previous one-step undo; the rollback helper is skipped on the snapshot failure path. | `plugin_editor.c:1380-1424` | Quick |
| C6 | A comment-only `.undo` is advertised forever but always reports `NO CHANGES`, because no-change compares rows, not bytes. | `plugin_editor.c:805-808,1759-1770` | Quick |
| C7 | Any stat failure after a failed open is classified ABSENT; an existing-but-unreadable config can be treated as absent and overwritten without a verified read. | `platform_psp2.c:236-248,414` | Moderate |
| C9 | Missing files count as ACTIVE and LOADED; HOME says `N PLUGINS ONLINE` while the PLUGINS header counts missing rows, and the row badge says MISSING. | `plugin_editor.c:258,346-360`; `plugins.c:414-426` | Quick |
| C10 | The restore preview ignores section membership: a KERNEL→SYSTEM move is "no change", and removing a KERNEL-only path produces no kernel warning. | `plugin_editor.c:45-48,378-437,1703-1720` | Moderate |
| C11 | Selection is an index; a restore re-sorts rows and the highlight can land on a different plugin, so the next Cross stages the wrong row. | `app/app.c:236-238,61-65`; `plugin_editor.c:293-324` | Quick |
| C12 | Dock focus is not revalidated; when battery gating makes CONFIRM unavailable, Cross silently does nothing until the user moves focus. | `plugin_flow.c:392-401`; `app/app.c:279-311` | Quick |
| C13 | A pad read returning 0 (no buffer) is not treated as failure; the zeroed pad reads as full up-travel and scrolls at maximum speed. | `app/app.c:450-453,50-53` | Quick |
| C14 | `hist_frame` receives 0 ms samples from `metrics_init` and from slow samples with no frame, drawing false dropped-frame dips. | `metrics.c:192-198` | Quick |
| C15 | Secondary power readings (temp/voltage/cycles/SOH) are accepted unchecked after percent succeeds; a failed read can render 0 °C or negative mV. | `platform_psp2.c:158-168`; `panels/power.c:54-64` | Moderate |
| T1 | The fake cannot produce `PLATFORM_READ_TOO_LARGE` (its 32 KiB ceiling equals the read cap) and cannot fail list-dir or produce storage errors; several dangerous branches are untestable. | `tools/fake/fake_platform.c:227-234` | Moderate |
| A2 | cap-test unloads `SceNet` even when it never loaded it, potentially breaking networking system-wide. | `vita-cap-test/src/main.c:172-174,260-262` | Quick |
| A3 | cap-test is UNSAFE with no API that requires it; its own comment says so. | `vita-cap-test/CMakeLists.txt:111-113` | Quick |
| K3 | `open_count++` is a read-modify-write race and `last_path` is a shared buffer written from every open; the app presents both as fact. | `vita-kernel-demo/kernel/main.c:18-19,29-31,37` | Moderate |
| K4 | The README documents `ksceIoOpen` / `0x75192972`, but the code hooks `_sceIoOpen` / `0xCC67B6FD`; the UI's "counts every kernel open" is false. | `vita-kernel-demo/README.md:29,111-116`; `app/main.c:80-82` | Quick |
| W2 | The source hash omits `--safe`, `sce_sys/` assets, `send.sh`, and other packaged inputs; asset-only changes reuse a build number. | `build.sh:35-36`; `CMakeLists.txt:109-114` | Quick |
| W3 | The source stamp lives in `build/`, so wiping `build/` (allowed by the docs) loses reuse; `tools/` is in the hash, so test-only edits burn numbers. | `build.sh:33-36` | Quick |
| W4 | The prune loop deletes only `*.vpk`; `.vpk.out`, `_param.sfo`, and pre-rename files accumulate. The directory currently holds b141/b142/b143-safe debris. | `build.sh:70-77` | Quick |
| W7 | `vita-cap-test` and `vita-kernel-demo` have no `docs/` tree, while the root README says every project does; AGENTS makes docs part of done. | root `README.md:221-224` | Moderate |
| W8 | PROJECTS.md's "Next" for kernel-demo (on-demand load test) cannot pass, and the root README gives the wrong UNSAFE rationale. | `PROJECTS.md:25`; `README.md:246-250` | Quick |

### LOW

| ID | Finding | File:line |
|---|---|---|
| C8 | `platform_recover_swap` short-circuits the second mount and ignores `-1`; a failed recovery is invisible. | `plugin_editor.c:825-827` |
| C16 | `plugins_scroll` has two writers: `keep_selection_visible` assigns directly, then calls the nav clamp. | `app.c:72-84`; `plugin_nav.c:120-124` |
| C17 | The PLUGINS header still says `FOR SESSION OPTIONS`; Triangle now only toggles the dock. | `panels/plugins.c:437` |
| C18 | `FREE USER MEM (MB)` renders as `FREE USER MEM  MB`; the font has no parentheses. | `panels/core.c:51`; `font.c` |
| C19 | `SCROLL_SPEED 900` is dead; the real speed is `INPUT_SCROLL_SPEED 520`. | `app.h:14` |
| C20 | Choice-modal Left/Right input remains though only the error modal exists. | `input.c:88-100` |
| C21 | `err_cap` is cast to `size_t` without a positive check; a negative cap would write far out of bounds. | `plugin_editor.c` error branches |
| C22 | `editor_stage_toggle` does not require `mode == editor_next_action(row)`. | `plugin_editor.c:1242-1289` |
| C23 | `PLUGINS ONLINE` counts title/system-app-scoped entries that are not loaded at rest. | `plugins.c:355-358`; `plugins_summary.c:124-133` |
| C24 | Transient dock errors never expire and can mask live warnings. | `app.c:122-131`; `plugin_flow.c:337-340` |
| C25 | The panel reimplements `editor_preview_diff` instead of calling it. | `panels/plugins.c:560-564` |
| C26 | Dead fields/macros/fixtures remain from the removed self-row/private-batch code. | `plugin_editor.h/.c`, `plugins.c`, `test_editor.c` |
| C27 | The firmware copy reads 31 bytes from a 28-byte `versionString` field. | `platform_psp2.c:191-198` |
| C28 | DESIGN gives two different PLUGINS scroll formulas; ARCHITECTURE's allowed-header table is stale. | `docs/DESIGN.md:68-70`; `docs/ARCHITECTURE.md:231-237` |
| T2 | The fake filesystem is case-sensitive while the target is not; a test pins MISSING for a path the console resolves. | `fake_platform.c:280-291` |
| T3 | No tests for the oversized-config, `.undo` rollback, no-live snapshot, section-only restore, comment-only undo, unrecovered-dock, or zero-frame branches. | `tools/test` |
| A4 | `gethostbyname`'s `h_addr_list[0]` and `h_length` are not checked before use. | `vita-cap-test/src/main.c:209-222` |
| A5 | A non-EEXIST `sceIoMkdir` failure is swallowed. | `vita-cap-test/src/main.c:88` |
| A6 | `vita2d_init` / `vita2d_load_default_pgf` are unchecked; a NULL font crashes the first frame. | `vita-cap-test/src/main.c:316-318`; `kernel-demo/app/main.c:31-33` |
| A7 | cap-test links `SceHttp`, `SceSsl`, `SceAppUtil` without calling them. | `vita-cap-test/CMakeLists.txt:80-91` |
| A8 | cap-test has no tests. | — |
| K5 | kernel-demo's app ignores `vita2d_init`/PGF failures. | `app/main.c:31-33` |
| K6 | kernel-demo README names `SceSysclibForDriver` and a length check that do not match the code. | `README.md:166,180` |
| K7 | kernel-demo has no tests. | — |
| K8 | Both smaller projects leave `.vpk.out` / `_param.sfo` debris. | `build.sh:58-62` |
| W5 | A manual CMake build defaults to `b0`, skipping the increment rule. | `CMakeLists.txt:27` |
| W6 | `send.sh` falls back to `build/cockpit.vpk`, which no build creates, and tells the user to select that name. | `send.sh:8-11,34` |
| W9 | AGENTS/wiki hardcode the author's absolute workspace path. | `AGENTS.md:179,208`; wiki `12` |

## 3. Dependency map

```
C1 (.old safety) ──unlocks──→ C4 (no-live recovery)
   │                         C7 (stat classification)
   └──→ C2 (oversized undo) ──→ C3 (unrecovered dock)
C3 ──needs──→ editor_can_commit predicate
C10 (section model) ──unlocks──→ C25 (panel uses one diff)
T1 (fake ceiling/hooks) ──unlocks──→ tests for C2, C5, C6, C14, T3
W1 (variant identity) ──unlocks──→ W2, W4 (one artifact)
K1 (strong import) ──blocks──→ any kernel-demo hardware test
K2 (shell-gated load) ──decision──→ W8 (status/next)
W7 (docs trees) ──unlocks──→ A3/K2 decisions recorded
```

## 4. Refactoring risk

| Change | Risk | Why |
|---|---|---|
| Config recovery changes (C1/C4/C7) | **High** | Boot-critical: a wrong fallback can overwrite or hide the user's config. Needs characterization tests and one fix per checkpoint. |
| `plugin_editor.c` split | **High** | 2,100-line module; not required for these fixes. Do not split while fixing recovery. |
| Dock state machine (C3/C12) | Medium | Touches `plugin_flow.c` and `app.c`; host tests exist. |
| Build script identity (W1-W4) | Medium | Changes artifact selection and `send.sh`; must verify both variants. |
| Kernel-demo redesign (K1/K2) | **Critical** | Build configuration and module boundary; requires a product decision (boot plugin vs retire). |
| cap-test timeouts (A1) | Medium | Network behavior on device; host tests need a fake socket layer. |

## 5. What is working well

- The dependency rule is real: `core/` is platform-free, `platform_psp2.c` owns
  every SDK file call, and `app/app.c` is the only other `psp2/*` user.
- The config transaction is byte-preserving and has a verified sibling, a
  rename-away swap, a read-back, `.original`/`.session`/`.undo`, and a stale-file
  check. The safety intent is strong; the defects are in edge-state recovery.
- The host suite is substantial (1,293 checks) and the preview harness renders
  the shipped composition.
- `boot_config.txt` is genuinely isolated: no source path names it.
- Artifact naming and the docs/decision log are disciplined; the problems are
  implementation gaps in the build script, not a missing rule.
- D99 removed the plugin surface cleanly; no dead plugin code remains in the
  shipped app.

## 6. Proposed fix order

1. **Recovery and data safety** — C1, C7, C2, C3, C4, C5, C6.
2. **Runtime correctness and UX** — C13, C11, C12, C9, C10, C14, C15.
3. **Build and release identity** — W1, W2, W3, W4, W5, W6.
4. **Smaller projects** — decide K1/K2 (likely mark kernel-demo `blocked` and
   document the boot-plugin risk class); A1, A2, A3.
5. **Tests, cleanup, docs** — T1-T3, C16-C28, A4-A8, K3-K8, W7-W9.

Estimated effort: ~25-30 hours of human work; the recovery and kernel-demo
decisions are the long poles. The COCKPIT fixes can be done without a file split.
