# Reddit announcement

Draft for a community release post. The body below is ready to paste; the
release and repo links point at the published repository.

## Before posting

1. Upload the eight images from [`screenshots/`](../screenshots/) as a gallery,
   in filename order. They are 960 × 544 host preview renders, not console
   captures — say so, it is already in the body.
2. Replace each `▸ IMAGE:` line with the matching image. In the new Reddit
   editor, delete the line and insert the image at that point; on old Reddit, a
   gallery post puts the images at the top, so drop the markers.
3. The release and repo links below are live. Update them if the release tag
   changes.
4. The build line is the b156 artifact. Rebuild and update it if the source
   changes before posting.

Keep the known-issues section last. It is the part that makes the rest
believable, and it is where the honest limits belong.

## Title options

1. `COCKPIT — a live system dashboard and taiHEN plugin manager for PS Vita (build 156)`
2. `I built a Cyberpunk-styled dashboard for the PS Vita that refuses to show fake numbers`
3. `COCKPIT: a PS Vita system monitor and config.txt manager, no plugins of its own`

---

## Body

**A live dashboard for the PS Vita, and a plugin manager that treats `config.txt`
as a transaction instead of a text file.**

**COCKPIT** reads the machine it is running on — CPU, memory, storage, power,
frame time — and draws it in the Cyberpunk 2077 HUD language. Not the anime:
the in-game HUD. Thin rules, one chamfered corner, small tracked labels over big
values. On a 5-inch screen that restraint matters more than bloom.

It is one userland app. No kernel plugin, no system plugin, no Quick Menu panel,
no FTP server. It installs a VPK and never adds its own line to your config.

▸ IMAGE: screenshots/01-home.png

### HOME

CORE is the panel I care most about. Per-core load for C0–C3, the average over
the cores that are actually active, context-switch and wakeup rates, and a
20-second trace. The memory pools are three rows because they are three separate
pools: USER, CDRAM, PHY.

STORAGE reads `ux0:` and `uma0:`, picking the unit from each volume's size.
POWER reads percentage, charging state, temperature, voltage, cycle count, and
health. SYSTEM has the firmware string, run time, and model. FRAME is measured
FPS and frame time with its own trace.

Scroll down and there is a summary of the plugin set, counted by load target.

▸ IMAGE: screenshots/02-home-plugins.png

### PLUGINS

The ledger resolves the active config the way taiHEN does — `ux0:` first, `ur0:`
fallback — and tells you which file it used. Every path is listed, active or
commented, grouped KERNEL / SYSTEM / SYSTEM APPS / ALL TITLES, one row per
unique path.

Each row carries two separate facts: what is running now (`LOADED`,
`NOT LOADED`, `MIXED`, `MISSING`, `NOT REACHED`, `BOOT`, `LOCKED`) and what will
run after the next boot (`NEXT ON` / `NEXT OFF`).

▸ IMAGE: screenshots/03-plugin-ledger.png

A listed file that is missing, or a config that needs attention, is named rather
than hidden:

▸ IMAGE: screenshots/06-config-warning.png

Cross stages the selected row. Nothing is written and no dialog opens. The dock
at the bottom lists the pending enables and disables and offers `DISCARD`,
`SAVE, REBOOT LATER`, and `CONFIRM & REBOOT`. Confirm writes the batch once and
requests a cold reset.

▸ IMAGE: screenshots/04-plugin-draft.png

`RESTORE ORIGINAL` and `RESTORE PREVIOUS SESSION` sit on the dock's second row.
A restore shows a plugin-level diff before anything is written, and
`UNDO LAST WRITE` opens the same preview for the last committed batch.

▸ IMAGE: screenshots/05-plugin-restore-preview.png

It does not install, delete, or download plugins. It toggles the lines that are
already in your file.

### The numbers

Every number on screen is real. If the platform cannot report it, the panel shows
`--`.

That is why there is no GPU load row and no applied ARM/GPU/BUS clocks. Userland
cannot read them. The ScePower getters return what this process asked for, not
what the hardware is running at. I would rather drop the row than print a number
I cannot defend.

Same rule for failures. A frozen or regressed cumulative counter shows `--` for
that rate, not `0/S`. A missing `uma0:` reads `N/A`; a `uma0:` that exists but
cannot be read shows its return code. This is a failed CPU read:

▸ IMAGE: screenshots/07-core-unavailable.png

### Guard rails

`boot_config.txt` is never opened, read, copied, or written. `taihen.skprx`,
`henkaku.skprx`, `yamt.skprx`, and `henkaku.suprx` are locked by name; a batch
that would drop one is refused and the path is named.

Both reboot paths are refused below 20 % while discharging, or when the power
read is invalid. The save path stays available.

▸ IMAGE: screenshots/08-reboot-blocked.png

Writes go through a sibling swap: write `.new`, verify, move the original to
`.old`, move `.new` into place, read back. A failed step restores the original,
and the message names the stage (`OPEN`, `DATA`, `CLOSE`, `VERIFY`, `MOVE`,
`RENAME`, `READBACK`, `RESTORE`, `UNDO`). Snapshots are `config.txt.original`
(written once, never overwritten), `config.txt.session` (each launch), and
`config.txt.undo` (one step).

If a reading on the panel looks wrong, that is a bug. The panel is supposed to
say `--` instead.

### Install

- **PS Vita on 3.65 with permanent Enso.** PSTV is out of scope.
- Copy the VPK to `ux0:data`, install it from VitaShell, launch it from
  LiveArea.
- The default build is **UNSAFE**, because `uma0:` capacity needs
  `sceIoDevctl`. `./build.sh --safe` builds the restricted variant; `uma0:`
  then reads `N/A`.
- Sticks or D-pad scroll. L/R switch tabs. Cross stages, Circle backs out,
  Triangle toggles dock focus.

### Known warnings and issues

- **Build 156 has not been run on my console yet.** b150 has, and it works.
  b156 adds the CORE CPU readout and a batch of review fixes. The failure mode is
  contained: if the idle clock turns out to be coarser than microseconds, CORE
  shows `--` instead of a wrong percentage. If you flash it, I would like to know
  what C0–C3 do at idle.
- **Firmware may read 3.74 on a 3.65 console.** The version string is spoofed on
  mine. The app follows 3.65 APIs and shows what the API returns, because that
  is the honest value.
- **No GPU figures.** Userland exposes neither GPU load nor applied GPU clocks.
  CDRAM/PHY and the FRAME strip are the whole GPU view.
- **The default eboot is UNSAFE.** That is a system-wide permission relaxation.
  Use `--safe` if you do not need `uma0:`.
- **The editor writes your real `config.txt`.** Staging does not. Confirm does.
  It leaves `.original`, `.session`, and `.undo` next to the file. If a write
  fails and cannot be restored, the app locks the editor and tells you not to
  reboot. Take that seriously.
- **A kernel plugin in the batch can stop boot.** The dock warns you and reminds
  you to hold L at power-on. That is your way back.
- **Only tested on 3.65 Enso.** Other firmware is untested. Do not be the first
  on a firmware you cannot recover.
- **These screenshots are host preview renders**, not console captures. The
  layout and type are the shipped ones; the readings in them are fixtures.
- **No network, audio, camera, or motion.** This is a dashboard and a config
  editor, nothing else.

Built with VitaSDK 2026.08 and libvita2d. 1,412 host checks and 40 preview scenes
pass, and the VPK contains only `eboot.bin` and `sce_sys/`.

**Download:** https://github.com/ghostmeson/vitacockpit/releases/tag/v1.00

**Source:** https://github.com/ghostmeson/vitacockpit

**Build:** `cockpit-v01.00-b156.vpk`, 132,734 bytes, SHA-256
`84c9d16c8c10d5c8ad918ff5a5177041780884e93829e3a1b5cf991ff0b6335c`
