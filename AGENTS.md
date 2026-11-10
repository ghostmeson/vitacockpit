# AGENTS.md — vita-cockpit

Operating contract for this repository. It is self-contained: the project owns
its toolchain assumptions, its docs, and its safety rules.

## What this project is

COCKPIT is one userland PS Vita app: a taiHEN `config.txt` plugin manager first,
with the VITALS system dashboard as its second tab and ABOUT as its third. It
ships no system plugin, kernel plugin, FTP server, Quick Menu panel, RCC/PAF
payload, or clock bridge (D99). The VPK contains `eboot.bin` and `sce_sys/`
only, and the app never adds a COCKPIT line to `config.txt`.

The product rule is that **every number on screen is real**. A value the
platform cannot report renders `--` or `N/A`, never a guess, a stale value, or a
zero.

## Scope: this repository only

This repository contains COCKPIT and nothing else. Do not inspect, edit, build,
or document a sibling project or another checkout. Every code, test, build, and
doc change stays inside this directory.

The repository is standalone. It depends on a VitaSDK installation, not on a
parent workspace: point `VITASDK` at the toolchain and keep it outside the
repository, where `.gitignore` would not see it.

## Read before changing anything

1. [`README.md`](README.md) — what it is and how to build it.
2. [`docs/PROJECT.md`](docs/PROJECT.md) — scope, status, roadmap, hardware handoff.
3. [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — layers, port, module contracts.
4. [`docs/DESIGN.md`](docs/DESIGN.md) — visual language and layout.
5. [`docs/DECISIONS.md`](docs/DECISIONS.md) — numbered decisions, including D99-D105.
6. [`docs/threads/2026-11-09-plugin-manager-first.md`](docs/threads/2026-11-09-plugin-manager-first.md)
   for the current work and its open hardware checks. The CPU thread and review
   under `docs/threads/2026-11-08-core-cpu.md` and
   [`docs/reviews/2026-11-08-core-cpu/report.md`](docs/reviews/2026-11-08-core-cpu/report.md)
   remain the record for b156.

## Target and identity

- PS Vita on **3.65 with permanent Enso**. PSTV is out of scope.
- The console reports **3.74** because its version string is spoofed. That is
  expected; NIDs, offsets, and API availability follow **3.65**.
- `sceKernelGetSystemSwVersion()`, the system UI, and COCKPIT's SYSTEM panel
  showing 3.74 are correct displays of a spoofed value. Do not "correct" them.

## Build, test, preview, send

From this directory:

```sh
export VITASDK="$HOME/vitasdk"   # or wherever VitaSDK is installed
export PATH="$VITASDK/bin:$PATH"

./build.sh                 # default UNSAFE eboot
./build.sh --safe          # restricted eboot, -safe artifact
./tools/test/run.sh        # host tests: metrics + editor + UI
./tools/preview/run.sh     # 51 PNG scenes in build/preview/
./send.sh <vita-ip>        # uploads the newest VPK over FTP
```

- The artifact is `build/cockpit-v<version>-b<build>[-safe].vpk`.
- The launcher PNGs (`sce_sys/icon0.png`, `livearea/contents/bg.png`, and
  `startup.png`) must be 8-bit indexed/colormapped, not RGBA. `file` must say
  `8-bit colormap`; RGBA fails installation with `0x8010113D` (D107).
- `BUILD_NUMBER` lives in this directory, never in `build/`. `build.sh` owns the
  increment and reuses the number only when the source hash is unchanged.
- The build directory holds exactly one VPK. `send.sh` has no fixed-name
  fallback and names the variant it uploads.
- Do not pass `-DCOCKPIT_BUILD=0`; CMake rejects it. A manual build must supply
  a real build number.

## Architecture rules

The dependency rule is binding:

- `src/core/` is policy. It must not include `psp2/*`, `vita2d.h`, a panel, or a
  UI header. CPU, memory, storage, power, identity, history, and the plugin model
  live here.
- `src/platform/platform_psp2.c` is the single Vita SDK adapter for device
  sampling and config I/O.
- `src/app/app.c` is the only other userland file allowed to include `psp2/*`
  (pad and process time).
- `src/gfx/draw.c` and `src/main.c` are the only files allowed to include
  `vita2d.h` (renderer adapter and composition root).
- `src/main.c` is the composition root. Nothing else constructs the renderer.
- `g_metrics` has one writer: `src/core/metrics.c`. `g_editor` has one writer:
  `src/core/plugin_editor.c`. Panels read and draw; they do not mutate.
- Check every return value. Degrade to `--`/`N/A`; never crash and never
  fabricate a reading.
- Name every constant that carries meaning. Keep implementation types and
  helpers out of public headers.
- No dead code, no unused links, and no function nothing calls.
- Comments explain why, especially platform quirks, rejected alternatives, and
  unverified claims.

## Unverified platform claims

Tag unverified behavior with `[C]`, isolate it in the adapter, and record it in
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) section 7 and
[`docs/PROJECT.md`](docs/PROJECT.md) section 5. A `[C]` sample must degrade to
`--`, not a wrong number.

The current `[C]` CPU claim: `SceKernelSystemInfo.cpuInfo[].idleClock` is treated
as microseconds. `core/metrics.c` rejects a delta larger than elapsed time,
which catches a finer unit; a coarser unit is not detectable from the counters,
so the hardware checklist treats a static ~100 % at idle as unavailable.

## Config-manager safety

The PLUGINS tab edits the active taiHEN `config.txt` and nothing else.

- Never open, edit, copy, or back up `ur0:tai/boot_config.txt` or any Enso boot
  file. Boot modules are locked by the three known names only.
- Never add a COCKPIT line to `*KERNEL`, never install a plugin, and never load a
  kernel module. The plugin surface is removed, not disabled (D99).
- Writes are byte-preserving and go through the verified sibling swap in
  `platform_write_config`: `.new` -> verify -> move the original to `.old` ->
  move `.new` into place -> read back. A failed step restores the original.
- Preserve an orphan `.old`; it can be the only surviving config. Only a real
  `ENOENT` stat failure means "absent"; every other stat failure is unreadable.
- Snapshots are `.original` (once), `.session` (per session), and `.undo`
  (one-step). Never rotate or delete them outside a save, restore, or completed
  undo.
- A failed write that cannot be restored is a repair state: offer UNDO, gate
  SAVE/CONFIRM, and keep rows read-only.
- Do not delete system directories. `ur0:`, `ux0:`, `ur0:data`, `ux0:data`,
  `ur0:tai`, `ux0:tai`, and `app0:` belong to the console.
- `UNSAFE` is the default only because `uma0:` capacity needs `sceIoDevctl`
  (D42). `./build.sh --safe` builds the restricted variant. Do not add an
  `UNSAFE` need without a decision.
- No network, audio, camera, motion, or FTP work belongs in this app.

## Tests and evidence

- `./tools/test/run.sh` links the real `core/` against `tools/fake/fake_platform.c`.
  Add tests for new policy branches, failure paths, and state transitions.
- `./tools/preview/run.sh` renders the shipped composition through the software
  vita2d shim. Add or update a scene when a panel's visible state changes.
- The fake is a contract model, not the Vita. A host check does not prove
  `sceIoRename`, `sceIoGetstat`, `sceKernelGetSystemInfo`, or any device
  behavior. Keep the unstripped `build/cockpit` ELF for hardware sessions.
- Do not mark the project `verified` without console evidence. `built` means it
  compiles and packages.

## Documentation and the board

Update the docs in the same turn as the change:

| Change | Update |
|---|---|
| Scope, status, next action | `docs/PROJECT.md`, `README.md` |
| Layer, port, platform claim | `docs/ARCHITECTURE.md` §7, `docs/PROJECT.md` §5 |
| Visual language or layout | `docs/DESIGN.md` |
| Reversed or new decision | `docs/DECISIONS.md` with a new number; never delete history |
| API behavior or artifact | `README.md`, `docs/PROJECT.md` §5 |
| Example source | `docs/examples/` |
| Historical plan | `docs/plans/` |
| GPU crash evidence | `docs/gpu-crash-study/` |
| Community announcement | `docs/reddit-post.md`, `screenshots/` |
| Thread or review record | `docs/threads/`, `docs/reviews/` |

`README.md` is the public entry point: it must stay accurate about what ships,
how to install it, and what is still unverified. The deeper docs carry the
detail and link back to it.

Run the link and code-fence check from this directory after doc edits:

```sh
python3 - <<'PY'
import os, re
bad = []
skip = {'build', '.git'}
for root, dirs, files in os.walk('.'):
    dirs[:] = [d for d in dirs if d not in skip]
    for f in files:
        if not f.endswith('.md'):
            continue
        p = os.path.join(root, f)
        txt = open(p, errors='replace').read()
        for m in re.finditer(r'\[[^\]]*\]\(([^)]+)\)', txt):
            link = m.group(1)
            if link.startswith(('http://', 'https://', '#')):
                continue
            target = link.split('#')[0]
            if target and not os.path.exists(
                    os.path.normpath(os.path.join(root, target))):
                bad.append((p, link))
        if txt.count(chr(96) * 3) % 2:
            bad.append((p, 'odd code fence'))
print('issues:', bad if bad else 'none')
PY
```

A job is not done until the build, tests, previews, docs, and README agree.

## Current state (2026-11-10)

- Artifact: `build/cockpit-v01.04-b172.vpk`, 146,932 bytes, SHA-256
  `6f4937e3f7881c308c297949f68aca52c7cd8a0fa85614cd8f9417fdd4721fec`.
- GitHub release `v1.04` carries this VPK.
- b166 (`v1.01`) is confirmed on a **PS Vita 1000 running 3.65 Enso** (user
  report, 2026-11-10). The project is `verified` for the app code. b172 adds the
  launcher icon and LiveArea art; it is built and packaged, and its bubble art
  has not been separately observed on the home screen.
- The CORE idle-clock unit remains `[C]`; a coarser-than-microseconds counter is
  caught only by the static-100 % hardware check.
