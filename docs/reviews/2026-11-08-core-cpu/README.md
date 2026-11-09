# CORE CPU code review — 2026-11-08

Run `20261108-105324-38da7151`, reviewed `1910e0f` against `a4ef5d4`.
The review was report-only; no code fixes were applied.

- **Verdict:** Ready with fixes
- **Team:** correctness, project-standards, testing, maintainability, adversarial
- **Cross-model pass:** not run; the host serving family was un-attestable under
  OpenCode/DeepSeek, so the in-process adversarial reviewer covered the lens.
- **Validator:** 8/8 findings confirmed in one batch.

## Artifacts

| File | Contents |
|---|---|
| [report.md](report.md) | The full rendered review |
| [metadata.json](metadata.json) | Run metadata and verdict |
| [synthesized-findings.json](synthesized-findings.json) | Merged, validated finding set |
| [validator-verdicts.json](validator-verdicts.json) | Independent verdicts |
| [full.diff](full.diff) | Reviewed diff |
| [files.txt](files.txt) | Changed-file list |
| [correctness.json](correctness.json), [project-standards.json](project-standards.json), [testing.json](testing.json), [maintainability.json](maintainability.json), [adversarial.json](adversarial.json) | Per-reviewer artifacts |

The intermediate merge mechanics are not archived.

## Resolution

The documentation corrections are in `d26190f`. The behavior and coverage items
were closed in `bbd6e67`:

- **#2** -- per-field `cpu_ctx_valid` / `cpu_wake_valid`; a zero, frozen, or
  regressed counter renders `--`, never `0/S`.
- **#3** -- `ui/core_text` formatter tests and the CPU-absent and two-core
  preview scenes.
- **#5** -- `cpu_active_mask` removed and the fake activity hook exercised by
  the rate tests.

**#1(b)** remains deferred by design: a coarser-than-microseconds `idleClock` is
not detectable from the counter, so the hardware checklist treats a static
100 % at idle as unavailable. The follow-up is recorded in
[../../threads/2026-11-08-core-cpu.md](../../threads/2026-11-08-core-cpu.md).
