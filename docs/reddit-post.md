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
4. The build line is the b164 artifact. Rebuild and update it if the source
   changes before posting.

Keep the known-issues section last. It is the part that makes the rest
believable, and it is where the honest limits belong.

## Title options

1. `COCKPIT — a taiHEN config.txt plugin manager for PS Vita, with a real-numbers dashboard (v1.01)`
2. `I built a PS Vita plugin manager that refuses to show fake numbers`
3. `COCKPIT v1.01: manage your taiHEN plugins without hand-editing config.txt`

---

## Body

**A taiHEN `config.txt` plugin manager for the PS Vita, with a system dashboard
behind it.**

**COCKPIT** opens on PLUGINS. It reads the active taiHEN config, lists every
plugin in it, shows what is running now and what will run after the next boot,
and lets you stage enables and disables as one reviewed batch. VITALS is the
second tab; ABOUT is the third.

It is one userland app. No kernel plugin, no system plugin, no Quick Menu panel,
no FTP server. It installs a VPK and never adds its own line to your config.

▸ IMAGE: screenshots/01-plugins.png

### PLUGINS first

The ledger resolves the active config the way taiHEN does — `ux0:` first, `ur0:`
fallback — and tells you which file it used. Every path is listed, active or
commented, grouped KERNEL / SYSTEM / SYSTEM APPS / ALL TITLES, one row per
unique path.

Each row carries two separate facts: what is running now (`LOADED`,
`NOT LOADED`, `MIXED`, `MISSING`, `NOT REACHED`, `BOOT`, `LOCKED`) and what will
run after the next boot (`NEXT ON` / `NEXT OFF`).

Cross stages a row. Nothing is written and no dialog opens. The dock lists the
pending enables and disables and offers `DISCARD`, `SAVE, REBOOT LATER`, and
`CONFIRM & REBOOT`. Confirm writes the batch once and requests a cold reset.

▸ IMAGE: screenshots/03-plugin-draft.png

`RESTORE ORIGINAL` and `RESTORE PREVIOUS SESSION` sit on the dock's second row.
A restore shows a plugin-level diff before anything is written, and
`UNDO LAST WRITE` opens the same preview for the last committed batch.

▸ IMAGE: screenshots/04-plugin-restore.png

It does not install, delete, or download plugins. It toggles the lines that are
already in your file.

### The launch card

Managing `config.txt` by hand is where a console stops booting. COCKPIT already
writes a session snapshot before you can touch a row, but that safety story was
invisible. Every launch now opens on a card that says what the app manages, what
it backed up, and why editing here is safer than a manual edit.

The card is honest about failure. If the session snapshot could not be written,
it says so. If the config is unreadable or too large, it says editing is locked
instead of promising a restore point. Cross or Circle continues; it stores no
acknowledgement, so it returns next session.

▸ IMAGE: screenshots/05-launch-card.png

### VITALS second

CORE shows per-core load for C0–C3, the average over the cores that are actually
active, context-switch and wakeup rates, a 20-second trace, and the three memory
pools: USER, CDRAM, PHY. STORAGE reads `ux0:` and `uma0:`, picking the unit from
each volume's size. POWER reads percentage, charging state, temperature, voltage,
cycle count, and health. SYSTEM has the firmware string, run time, and model.
FRAME is measured FPS and frame time with its own trace.

▸ IMAGE: screenshots/02-vitals.png

### ABOUT third

ABOUT names the build, shows the release label and build number, and offers the
only console action: REBOOT, behind a confirmation. If a staged batch or a
restore preview is pending, the confirmation says it will be discarded and
points you back to PLUGINS; if a batch is already saved, it says the reboot
applies it. The confirmation re-checks the battery and repair gates every frame.

There is no POWER OFF button. Userland cannot request a shutdown, so the page
prints the console's real path: hold the power button for two seconds, then
choose Power Off.

▸ IMAGE: screenshots/06-about.png

▸ IMAGE: screenshots/07-reboot-confirm.png

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

▸ IMAGE: screenshots/08-core-unavailable.png

### Guard rails

`boot_config.txt` is never opened, read, copied, or written. `taihen.skprx`,
`henkaku.skprx`, `yamt.skprx`, and `henkaku.suprx` are locked by name; a batch
that would drop one is refused and the path is named.

The dock and ABOUT refuse a reboot below 20 % while discharging, when the power
read is invalid, and in a repair state (`REPAIR WITH UNDO FIRST` or
`RECOVERY FAILED`). The save path stays available.

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
- L/R step PLUGINS, VITALS, ABOUT. Sticks or D-pad scroll. Cross stages a row,
  opens the ABOUT confirmation, or activates a dock action. Circle backs out.

### Known warnings and issues

- **Build 164 has not been run on my console yet.** b150 has, and it works. b164
  adds the plugin-manager-first tabs, the launch card, and the ABOUT reboot gate.
  The new screens are the part I most want tested; if something looks wrong,
  please say which tab and what the screen showed.
- **The launch card describes the backup it actually wrote.** If your config is
  unreadable or too large, expect the locked body, not a restore promise.
- **ABOUT's REBOOT is the only action on the page.** A staged batch is discarded
  only after the confirmation; a saved batch is applied at the next boot. The
  power-off line is the console's path, not an app request.
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
- **No network, audio, camera, or motion.** This is a plugin manager and a
  dashboard, nothing else.

Built with VitaSDK 2026.08 and libvita2d. 1,481 host checks and 51 preview scenes
pass, and the VPK contains only `eboot.bin` and `sce_sys/`.

**Download:** https://github.com/ghostmeson/vitacockpit/releases/tag/v1.01

**Source:** https://github.com/ghostmeson/vitacockpit

**Build:** `cockpit-v01.01-b164.vpk`, 135,549 bytes, SHA-256
`0df938415906463a8fc8f9dc252af56940e3b98136dd81c3d34ab96d3a7c057e`
