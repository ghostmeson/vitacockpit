# Plugin-manager-first COCKPIT (v1.01)

**Date:** 2026-11-10
**Status:** built; the hardware pass is pending.

## Goal

Turn COCKPIT from a dashboard-first app into a taiHEN `config.txt` plugin manager
first, with the system readout as the second screen. Add a launch safety card
that tells a new user what the app backs up, an ABOUT page with the build
identity and a guarded reboot, and publish the result as v1.01.

## What shipped

- PLUGINS is the first tab and the launch screen. VITALS is second, ABOUT third.
  L/R steps one tab and wraps.
- The launch safety card is a blocking gate. It is composed from the editor's
  actual launch result: snapshot written, failed snapshot, unreadable/oversized,
  no config, and the read-OK locked states. It is acknowledged with Cross or
  Circle, stores nothing, and returns next launch.
- The modal layer has three kinds: the error interrupt, the safety card, and the
  reboot choice. One scrim sits under all three.
- ABOUT shows the product, the release label `1.01`, the build number, the guarded
  REBOOT, and the real power-off path. Userland has no shutdown request, so the
  page prints the hold-power path instead of a button that would lie.
- The reboot gate is one function: invalid power, below 20 % while discharging,
  `write_unrecovered`, then `recover_failed`. ABOUT shows the reason instead of a
  confirmation; an open confirmation re-checks the gate every frame. A staged
  batch or preview is discarded only after the confirmation says so; a saved
  batch applies at the next boot.
- `tools/gen-build-id.sh` is the one identity source for the VPK name, the app,
  the host tests, and the previews. The package keeps `01.01`; ABOUT shows `1.01`.
- The docs, README, screenshots, and announcement use the plugin-manager-first
  framing. Earlier decision entries, threads, and review records keep their
  wording.

## Verification

- `./tools/test/run.sh`: 1,474 checks, 0 failures.
- `./tools/preview/run.sh`: 54 scenes, busiest 6,712 draw calls.
- `build/cockpit-v01.01-b162.vpk`: 135,495 bytes, SHA-256
  `eb878f15af5bb31a4bd43704c2cfd79e5e73bbaa485365230fa69a924068aaf1`.
- The VPK contains `eboot.bin` and `sce_sys/` only.
- GitHub release `v1.01` carries the VPK.

## Open

- The hardware pass in `docs/PROJECT.md` §5: the three tabs, the card on the
  console, ABOUT's reboot and power-off copy, a plugin round trip, and the
  repair-state block.
- The CORE `idleClock` unit remains `[C]`; a coarser-than-microseconds counter
  is caught only by the static-100 % idle check.
