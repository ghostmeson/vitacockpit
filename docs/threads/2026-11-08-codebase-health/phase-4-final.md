# Phase 4 — Final State

Date: 2026-11-08

## 1. What changed

| Area | Before | After |
|---|---|---|
| Config recovery | orphan `.old` could be deleted; oversized UNDO always refused; no-live ORIGINAL/SESSION unreachable; comment-only UNDO never committable | `.old` is recovered or the write fails closed; oversized/no-live/comment-only UNDO paths work; failed `.undo` snapshots preserve the previous one |
| Unrecovered write | dock showed dead SAVE/CONFIRM and omitted UNDO; rows stayed editable | repair state offers UNDO; SAVE/CONFIRM are gated; rows are read-only |
| Restore selection | numeric index could land on another plugin after re-sort | selection is preserved by path |
| Dock focus | could stay on an unavailable action, leaving Cross inert | focus revalidates after the 10 Hz sample |
| Missing files | counted as ACTIVE/LOADED; HOME and PLUGINS disagreed | excluded from ACTIVE/LOADED; HOME says `PLUGINS LISTED` |
| Preview | section moves invisible; KERNEL removals lost the warning | `WILL MOVE`; KERNEL moves/removals warn |
| Input | a zero-buffer pad read became full up-travel; choice-modal Left/Right was dead | `<= 0` centres all axes; modal consumes only accept/cancel |
| Frame history | fabricated 0 ms samples | only real frame samples |
| Power | secondary readings accepted unchecked | per-field validity; `--` per failed field |
| Build identity | safe/default shared a number; assets not hashed; stamp in `build/`; stale `.vpk.out`/`_param.sfo` | variant + assets in the hash; root stamp; one VPK; CMake rejects `b0`; `send.sh` names the variant |
| cap-test | UNSAFE without a requiring API; blocking DNS/connect/recv; unconditional SceNet unload | safe authid; 5 s resolver/connect/send/recv bounds; paired unload; docs + packaging test |
| kernel-demo | presented as "built, next: hardware test" | `blocked`; both launch blockers documented; atomic counter + path critical section; correct NID docs; docs + packaging test |
| Project docs | cap-test/kernel-demo had no `docs/`; root README claimed they did | both have `PROJECT`/`ARCHITECTURE`/`DECISIONS`; root README true |

## 2. Health score delta

| Area | Before | After |
|---|---|---|
| COCKPIT architecture | 9/10 | 9/10 |
| COCKPIT correctness | 6/10 | 8/10 |
| COCKPIT recovery/data safety | 4/10 | 8/10 |
| COCKPIT tests | 7/10 | 9/10 |
| COCKPIT build/release identity | 5/10 | 9/10 |
| COCKPIT docs | 7/10 | 8/10 |
| `vita-cap-test` | 4/10 | 6.5/10 |
| `vita-kernel-demo` | 3/10 | 5/10 (blocked, honestly documented) |
| Workspace build/docs | 5/10 | 9/10 |
| **Overall** | **5.4/10** | **7.7/10** |

The kernel-demo score is low because the project cannot run; the documentation
now says so instead of scheduling a doomed hardware test.

## 3. Verification

- COCKPIT default `b150`: 130,853 bytes, SHA-256
  `5143618f9affa68c275c1406819b32377c4662c3fccd143d0a9228a09f53555c`,
  `eboot.bin` + `sce_sys/` only.
- COCKPIT safe `b150`: clean out-of-tree build.
- Host: 1,371 checks, 0 failures; 38 preview scenes, busiest 5,429/8,000.
- cap-test: `b6`, safe authid, packaging test passes.
- kernel-demo: `b8`, packaging test passes, runtime blocked.
- Docs: link/fence check `none`; `git diff --check` clean.
- Hardware: **not run**. b150 has not been on the console.

## 4. Remaining work

| Item | Priority | Why |
|---|---|---|
| Flash b150 on the 3.65 console and run the 14-step checklist plus the new recovery regressions | HIGH | Only hardware can confirm `sceIoRename`/`sceIoGetstat`, the undo repair paths, and the dock feel. |
| Decide kernel-demo: retire, file/shared-memory exchange, or boot plugin | HIGH | The current VPK cannot launch; a decision is needed before more work. |
| Run cap-test `b6` on hardware | MEDIUM | The safe authid and 5 s network bounds need device evidence. |
| Add a host fake for the remaining adapter edges (device rename semantics, stat error codes) | MEDIUM | The fake still models the contract, not the Vita filesystem. |
| Add a host test for the app-level pad-read fallback | LOW | The app layer is not linked in the host suite. |
| Consider splitting `plugin_editor.c` | LOW | It is ~2,100 lines; do it as a separate high-risk refactor, not during recovery work. |

## 5. Closeout

The review and fixes are recorded in:
- `phase-1-discovery.md` — system map and flows.
- `phase-2-diagnosis.md` — findings, scores, dependency map.
- `phase-3-fixes.md` — fix log, tests, gates.
- `phase-4-final.md` — this before/after and remaining work.

The COCKPIT decisions are D100–D102; the cap-test decisions are D1–D3; the
kernel-demo decisions are D1–D4. The live project board is `PROJECTS.md`.
