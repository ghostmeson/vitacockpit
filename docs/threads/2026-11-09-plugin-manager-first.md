# Plugin-manager-first COCKPIT (v1.01 → v1.03)

**Date:** 2026-11-10
**Status:** verified on a PS Vita 1000 with 3.65 Enso (b166 app code); v1.03
adds the installable launcher art.

## Goal

Turn COCKPIT from a dashboard-first app into a taiHEN `config.txt` plugin manager
first, with the system readout as the second screen. Add a launch safety card
that tells a new user what the app backs up, an ABOUT page with the build
identity and a guarded reboot, and publish the result as v1.01. Then replace the
SDK placeholder launcher art with a COCKPIT mark and record the hardware pass.

## What shipped

- PLUGINS is the first tab and the launch screen. VITALS is second, ABOUT third.
  L/R steps one tab and wraps.
- The launch safety card is a blocking gate. It is composed from the editor's
  actual launch result: snapshot written, failed snapshot, unreadable/oversized,
  no config, and the read-OK locked states. It is acknowledged with Cross or
  Circle, stores nothing, and returns next launch.
- The modal layer has three kinds: the error interrupt, the safety card, and the
  reboot choice. One scrim sits under all three.
- ABOUT shows the product, the release label, the build number, the guarded
  REBOOT, and the real power-off path. Userland has no shutdown request, so the
  page prints the hold-power path instead of a button that would lie.
- The reboot gate is one function: invalid power, below 20 % while discharging,
  `write_unrecovered`, then `recover_failed`. ABOUT shows the reason instead of a
  confirmation; an open confirmation re-checks the gate every frame. A staged
  batch or preview is discarded only after the confirmation says so; a saved
  batch applies at the next boot.
- `tools/gen-build-id.sh` is the one identity source for the VPK name, the app,
  the host tests, and the previews.
- The docs, README, screenshots, and announcement use the plugin-manager-first
  framing. Earlier decision entries, threads, and review records keep their
  wording.

## v1.02 — launcher assets and hardware pass

- `sce_sys/icon0.png` is now 128 × 128: a black field, the chamfered yellow HUD
  frame, a heavy yellow `C`, and a cyan scanline with an end block. It replaces
  the SDK `Hello World!` placeholder.
- `sce_sys/livearea/contents/bg.png` (840 × 500) and `startup.png` (280 × 158)
  use the same mark. The startup image adds the `COCKPIT` wordmark and
  `TAIHEN CONFIG MANAGER`.
- `VITA_VERSION` is `01.02`; the package is `cockpit-v01.02-b168.vpk`. The app
  behind the assets is the b166 code the user tested.
- The user reports b166 (`v1.01`) runs on a **PS Vita 1000 with 3.65 Enso**
  (2026-11-10). The project is `verified` for that app code.
- **Installer failure:** the three assets were 8-bit-per-channel RGBA PNGs.
  VitaShell rejected the VPK with `0x8010113D`; the artwork was fine, the PNG
  format was not.

## v1.03 — installable launcher assets

- The three assets are now 8-bit indexed/colormapped PNGs with an opaque RGB
  palette, matching the format of the SDK's original icon. The RGBA source was
  quantized to 256 colours with no dithering, then stripped to
  `IHDR/PLTE/tRNS/IDAT/IEND`.
- `VITA_VERSION` is `01.03`; the package is `cockpit-v01.03-b170.vpk`. The app
  code is still b166.
- **The VPK installs now.** The icon and LiveArea art are the same design as
  v1.02, only re-encoded.

## Verification

- `./tools/test/run.sh`: 1,491 checks, 0 failures.
- `./tools/preview/run.sh`: 51 scenes, busiest 6,712 draw calls.
- `build/cockpit-v01.03-b170.vpk`: 147,044 bytes, SHA-256
  `fa4a9847dd1e8a4983c763309366c18b6c0e87d89c034c0670f3be71e051e6d0`.
- The VPK contains `eboot.bin` and `sce_sys/` only, including the three launcher
  assets above. All three are 8-bit colormap, non-interlaced.
- GitHub release `v1.03` carries the VPK. `v1.02` is retained as the superseded
  release with the bad PNG format.
- `[V]` b166 runs on a PS Vita 1000 with 3.65 Enso (user report).
- `[C]` the new bubble art has not been separately observed on the home screen.

## Open

- The CORE `idleClock` unit remains `[C]`; a coarser-than-microseconds counter
  is caught only by the static-100 % idle check.
- The regression checklist in `docs/PROJECT.md` §5 remains the record for a
  future pass; v1.03 changes only the launcher assets.
