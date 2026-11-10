# Project

What COCKPIT is, what it is not, and where it stands.

---

## 1. Purpose

A taiHEN `config.txt` plugin manager first, with a live machine readout behind
it. PLUGINS is the landing screen: it lists the active config, shows what is
running now and what will run after the next boot, and stages enables and
disables as one reviewed batch. VITALS is the instrument panel; ABOUT
identifies the build and offers the guarded reboot.

The product rule is that **every number on screen is real**. A fabricated
metric would poison the credibility of the whole panel, so anything the
platform cannot report is omitted rather than approximated.

COCKPIT is one userland app. It ships no system plugin, no kernel plugin, no
FTP server, and no Quick Menu component (D99). It installs nothing outside its
own VPK, and it never adds a COCKPIT line to `config.txt`; the manager only
edits lines that are already there.

## 2. Scope

### v1 — in

| Region | Contents |
|---|---|
| Header | wordmark, clock, PLUGINS / VITALS / ABOUT tabs |
| PLUGINS (first tab) | the full config ledger with a staged working set, an inline transaction dock, launch-set and next-boot badges, split rockers, the two restore previews, contextual last-write undo, and the config snapshots |
| Launch safety card | the active config path, what this session backed up, why editing through COCKPIT is safer, and the real locked/no-config branches |
| VITALS (second tab) | CORE: per-core CPU load, active-core average, context-switch/wakeup rates, 20 s CPU-load trace, free user / CDRAM / physical-contiguous pools. STORAGE: `ux0:`, `uma0:` used and total, with the unit chosen from each volume's size. POWER: percent, charging state, temperature, voltage, cycles, health. SYSTEM: firmware string, app run time, device model. PLUGINS summary: loaded count, group counts, config warning. FRAME: measured FPS, frame time, 20 s trace |
| ABOUT (third tab) | product name, a short description, the release label and build number, the guarded REBOOT action, and the console's real power-off path |

The wall clock and the wordmark are header chrome, not a panel, so identity
appears exactly once on screen.

### v1 — out

- Network, audio, camera, motion.
- Any system, kernel, or app-local `.suprx` / `.skprx`. The applied-clock bridge
  and the shell plugin are removed, not disabled (D99). CORE has no applied
  ARM/GPU/BUS row: the userland request is not the applied value, and the project
  does not print a number it cannot stand behind. It also has no GPU-load row;
  userland exposes neither GPU load nor applied GPU clocks. The CDRAM/PHY pools
  and the pinned FRAME strip are the GPU view (D103).
- An FTP server, a Quick Menu panel, an RCO, a manifest, or an install prompt.
- Installing, deleting, or downloading plugin files.
- Editing, reading, copying, or backing up `ur0:tai/boot_config.txt` or any Enso
  boot file. The app never opens it; boot modules are locked by the three known
  names only.
- Manual editing of comments or sections. A toggle changes only `#` markers; a
  restore replaces the file with the snapshot bytes.
- A fourth tab, plugin presets, a line-level diff, or a draft that survives a
  restart. The restore preview is plugin-level and replaces the whole file.
- A POWER OFF button. Userland cannot request a shutdown; ABOUT prints the
  console's real hold-power path instead.

### Later

App and bubble inventory, per-title storage attribution, plugin presets,
network state, screenshot browser, theme switching. The panel contract exists so
these are additive, not invasive. See §7.

## 3. Target and constraints

- PS Vita on firmware 3.65 with permanent Enso CFW.
- PSTV is out of scope; its differences are not handled.
- Unsafe eboot. Reading `uma0:` capacity needs a call the safe authid refuses,
  so the app ships `UNSAFE` (D42); `build.sh --safe` builds the restricted
  variant.
- The user reports b166 (`v1.01`) runs on a PS Vita 1000 with 3.65 Enso
  (2026-11-10). b172 adds only the launcher icon and LiveArea art; the app code
  is the tested b166.
- The product is one app. A plugin manager that rewrites `config.txt` is enough
  surface; adding a resident process or a boot hook increased the failure cost
  without improving the manager.

The full constraint table, with `[V]`/`[C]` verification status for each
platform claim, is in [ARCHITECTURE.md §7](ARCHITECTURE.md#7-platform-claims-and-their-status).

## 4. Build and deploy

```sh
./build.sh                 # cmake configure + build
./send.sh 192.168.1.42     # upload the newest VPK over FTP
```

`build.sh` uses `$VITASDK` when it is set, then a `vitasdk/` directory beside
the checkout, then `$HOME/vitasdk`. The VPK contains `eboot.bin` and `sce_sys/` only. `./build.sh --safe`
builds the restricted eboot. On the Vita, open VitaShell, press Select to start
its FTP server, then run `send.sh` with the address it shows.

Scroll the panel area with the D-pad or the sticks. The header stays pinned; on
VITALS the FRAME strip is pinned too, while PLUGINS uses the bottom of the screen
for its transaction dock. A thin bar on the right shows position. Press L or R to
step PLUGINS, VITALS, ABOUT; the step wraps at either end. The VITALS `PLUGINS`
block is read-only: it counts the launch set, and it sits below the fold, so
scroll down to reach it. Pending edits are shown only on the PLUGINS tab that
made them.

Every launch opens on the safety card, over PLUGINS. It names the active
`config.txt`, the snapshot this session wrote, and why editing through COCKPIT is
safer than a manual edit. Cross or Circle acknowledges it; no acknowledgement is
stored. ABOUT carries the version and build, the guarded REBOOT, and the real
power-off path. The reboot confirmation re-checks the battery and repair gates
every frame, and a staged batch is discarded only after the confirmation says so.

On PLUGINS, Cross stages the selected row in memory. It does not write
`config.txt` and does not open a dialog. The row title, the split rocker, the
ACTIVE/DISABLED counts, and the dock summary all move together; the launch badge
does not. The pinned dock lists the pending enables and disables with
`DISCARD`, `SAVE, REBOOT LATER`, and `CONFIRM & REBOOT` as 34 px pressable
slabs; `RESTORE ORIGINAL` and `RESTORE PREVIOUS SESSION` sit on its second row
as 28 px slabs. The commit is a solid yellow slab; the other actions are dark
with a bottom ledge. `CONFIRM & REBOOT` writes the
batch once, takes one `.undo` snapshot, and requests a cold reset. `SAVE, REBOOT
LATER` writes the same batch and leaves `SAVED - REBOOT TO APPLY`: the plugin
processes already running stay loaded until the Vita restarts. The reboot paths
are refused below 20 % battery while discharging, or when the power read is
invalid; the save path then becomes the primary action.

The sticks scroll smoothly. The D-pad moves the row selection and the dock
actions: Down from the last row enters the dock, Up returns, Left/Right selects
an action, Cross activates it, Circle backs out, and Triangle toggles dock
focus. L/R still switch tabs. The restore preview is read-only.

A restore shows a plugin-level diff against the working set: `+` / `-` markers,
`WILL ENABLE`, `WILL DISABLE`, `WILL REMOVE`, and unchanged rows dim. Confirming
replaces the whole file with the snapshot bytes, including comments and
sections. A snapshot that would remove or disable a BOOT module or a locked
HENKAKU path is refused and the path is named; a snapshot that matches the
working set says `NO CHANGES - NOTHING TO RESTORE`. Neither writes anything.
After a save, the dock offers `UNDO LAST WRITE` while `config.txt.undo` differs
from the live file; it opens the same preview and removes the snapshot only
after a verified undo.

`config.txt.original` and `config.txt.session` are the two stable restore
points; `config.txt.undo` is the one-step contextual copy. A commit replaces
`.undo`. If a write fails, the message is inline (`WRITE FAILED <stage>`), the
draft or preview stays, and the file is unchanged or recovered. Only an
unrecoverable write opens the `DO NOT REBOOT` interrupt. `boot_config.txt` is
never opened, read, copied, renamed, or written.

Install by selecting the VPK in `ux0:data` inside VitaShell and pressing Cross.
The bubble appears on LiveArea.

### Every build gets a new artifact name

**Rule: no two builds may share a filename.** A VPK that overwrites its
predecessor cannot be identified after the fact — not on the console, not in
`ux0:data`, not in a bug report. This is not optional bookkeeping; it is what
makes "which build is on the Vita?" answerable.

The artifact is named:

```
build/cockpit-v<version>-b<build>.vpk        e.g. cockpit-v01.04-b172.vpk
```

| Part | Source | Changes |
|---|---|---|
| `version` | `VITA_VERSION` in `CMakeLists.txt` | only on a release, and it must stay `##.##` because it also becomes `APP_VER` |
| `build` | `BUILD_NUMBER`, incremented by `build.sh` | **every build**, automatically |

The split is deliberate. `APP_VER` has to be `##.##`, so a per-build version
would either break the SFO or lie about the release. The build number carries
per-build identity; the version carries release identity. Both appear in the
filename, so one glance identifies the artifact.

Mechanics:

- `BUILD_NUMBER` lives in the project root, **not** in `build/`. Wiping the build
  directory must not restart numbering into a collision with VPKs already copied
  to the console.
- `build.sh` hashes the sources. Unchanged sources reuse the previous number;
  changed sources increment it. Rebuilding to check for warnings therefore does
  not burn a number.
- Only the newest VPK is kept. Superseded artifacts are deleted after every
  build, so the directory always holds exactly the one to flash.
- `send.sh` uploads the newest `build/cockpit-v*.vpk`. Old builds are left alone
  so several can sit in `ux0:data` side by side.
- A release that must keep a fixed name (a store upload, say) is a deliberate
  exception and should say so in `DECISIONS.md`; it is not the default.

Do not hand-edit `BUILD_NUMBER` downward to reuse a number. If numbering has to
restart, delete the stale VPKs first and record why.

## 5. Verification status

The project rule: a claim is `[V]` only when it has been
checked against a primary source or observed on hardware, and `[C]` when it is
community knowledge.

| Area | Status |
|---|---|
| Builds and packages | `[V]` clean default and safe builds, 0 compiler warnings; `cockpit-v01.04-b172.vpk` is 146,932 bytes, SHA-256 `6f4937e3f7881c308c297949f68aca52c7cd8a0fa85614cd8f9417fdd4721fec`; the VPK contains `eboot.bin`, `sce_sys/` (including the new `icon0.png`, `bg.png`, and `startup.png`), and no other file; the unstripped ELF is kept as `build/cockpit` |
| Host tests (sampling, plugin model, draft/commit, restore, undo, input, dock flow, launch card, ABOUT text) | `[V]` `tools/test/run.sh`, 161 metrics + 1,264 editor + 66 UI checks, 0 failures |
| Launcher PNG format | `[V]` `icon0.png`, `bg.png`, and `startup.png` are 8-bit colormap, non-interlaced; an RGBA `icon0.png` made VitaShell fail the v1.02 install with `0x8010113D` (D107). `[C]` the v1.04 install retry. |
| Plugin list (`ux0:` first, `ur0:` fallback) | `[V]` host harness keeps a 19-entry real config in full and groups it as KERNEL 9 / SYSTEM 8 / SYSTEM APPS 1 / ALL TITLES 1; host tests cover BOM, commented entries, long names, section mapping, priority merge, the 64-entry ceiling, full-path duplicate rows, `*!` halts, lock/warn policy, the two snapshots, and the exact `#` edit |
| Staged transaction and restore | `[V]` host tests cover stage/discard/commit, one `.undo` per batch, the stale-file refusal, the plugin-level restore diff, `WILL REMOVE`, the locked-drop and no-change refusals, the read-only refusal for an unreadable or truncated live config, the `*!` halt before a protected path, the undo repair after an unrecovered write, the removed-path pending truth, the failed-save `.undo` preservation, the saved/last-write truth table, focus and the battery gate. `[C]` the end-to-end flow is unverified on hardware. |
| Memory, CPU, storage, power, identity APIs declared in SDK | `[V]` header inspection; `sceKernelGetSystemInfo` is a userland `SceLibKernel` export |
| CPU idle-clock unit and 3.65 availability | `[C]` `SceKernelSystemInfo.cpuInfo[].idleClock` is treated as microseconds. A delta larger than elapsed time is refused; a coarser unit is not detectable from the counters, so a static 100 % at idle is treated as unavailable. The call has not run on this console. |
| `SceIoDevInfo` layout | `[V]` `psp2common/kernel/iofilemgr.h`, 24 bytes |
| `sceIoDevctl("ux0:", 0x3001, …)` command | `[V]` `psp2/io/devctl.h` demonstrates it verbatim |
| Storage result unit (bytes) | `[C]` — the devctl byte unit remains unconfirmed |
| Device model codes | `[V]` `psp2common/kernel/sysmem.h` |
| Screen layout and typography | `[V]` host preview render, 51 states, busiest 6,712 draw calls against the 8,000 budget; a three-tab header, the VITALS-only FRAME strip, the launch card, ABOUT, the reboot confirmation, the off-left/on-right rocker, the 12 px dock padding, and CORE/POWER outlines (D53, D54, D77, D78, D81, D83, D99) |
| Launch card, three tabs, ABOUT | `[V]` host tests cover tab wrap, ABOUT inertness, the four card bodies, the reboot gate, and the confirmation copy; the preview renders the success, failed-snapshot, no-config, and unreadable card states, ABOUT allowed/blocked/unknown, and the four confirmation states. `[V]` 2026-11-10: the user reports b166, which contains these screens, runs on a PS Vita 1000 with 3.65 Enso. |
| Renders on hardware | `[V]` 2026-11-02 (b13): text, memory, power, identity and frame all draw on the console. 2026-11-04 (b124): the app and the PLUGINS screen draw. 2026-11-08: the user reports b150 runs on the console; CORE was memory-only. 2026-11-10: the user reports b166, with the plugin-manager-first tabs, the launch card, and the ABOUT reboot gate, runs on a PS Vita 1000 with 3.65 Enso. b172 adds the launcher art only. |
| b74 startup crash | `[V]` C2-12828-1; the core names `COCKPIT01`, stop reason `0x30003`, PC `0`, and r0 `0x812c0ae4` (`&reading` at `platform_read_clocks+0x2e`). The weak import and the whole clock path are gone (D99). |
| Config write path | `[C]` the adapter's rename-away swap and live read-back are implemented and build; host tests exercise the same transaction in the fake, not the SDK calls, and the target's rename semantics are unverified. A failure names the stage (`OPEN`, `DATA`, `CLOSE`, `VERIFY`, `MOVE`, `RENAME`, `READBACK`, `RESTORE`, `UNDO`). |
| Restore reads | `[V]` host tests read `.original`, `.session`, and `.undo` through the same port; `[C]` the SDK file reads are not exercised on hardware. |
| Cold reset | `[V]` `scePowerRequestColdReset` is declared in `psp2/power.h`; `[C]` that an UNSAFE userland eboot may perform it on 3.65 Enso from the ABOUT confirmation |
| Power-off instruction | `[V]` the official Vita guide: hold the power button for two seconds, then choose Power Off; `[C]` the exact on-console path on this 3.65 Enso unit |
| STORAGE reads | `[V]` both volumes report on hardware under `UNSAFE` (D42); `ux0:` also works on the safe build via `sceAppMgrGetDevInfo` (D38) |
| Behaviour not listed above | unverified |

The SYSTEM panel shows **FW 3.74** because the console's version string is
spoofed; the machine is on 3.65 Enso. That is the value the API returns, so it is
correct to display. See `AGENTS.md` § Target and identity.

The host tests (`tools/test/run.sh`) link the real `core/` against a fake port and
assert the alert policy at its boundary, the CPU idle-delta policy, active
mask, and per-field rate validity, the failed-read path, the unit conversions,
the history ring, the config
model, the launch/working/next-boot split, the staged draft, the single-write
commit, the two restore points and the contextual undo, the locked-drop and
no-change refusals, the read-only restore refusal, the `*!` halt safeguard, the
undo repair, the removed-path pending truth, the failed-save `.undo`
preservation, interrupted-swap recovery, the ux0-unreadable rule, the exact `#`
edit, the write transaction and its restore path, the shared draft/restore
commit protocol, the dock state/action/focus
policy, the navigation routing and SAVE/CONFIRM mapping, the input reducer, the
three-tab wrap, the launch-card bodies, the shared reboot gate, and the ABOUT
confirmation copy. The host preview (`tools/preview/run.sh`) renders the shipped
composition against
the same fake, 51 states with the busiest at 6,712 draw calls. Neither proves
anything about the device: every reading in them is synthetic.

### Hardware handoff

Status: b166 (`v1.01`) has run on a **PS Vita 1000 with 3.65 Enso** (user report,
2026-11-10). The project is `verified` for that build. The checklist below is
retained as the regression record; b172 adds only the launcher icon and LiveArea
art, so its app behavior is the tested b166. The new bubble art is built into
the VPK but has not been separately observed on the home screen.

The next regression pass is to flash `build/cockpit-v01.04-b172.vpk` and run this
checklist. No item below is claimed by the host suite.

1. Flash the VPK and launch COCKPIT. Confirm PLUGINS is the first tab and the
   landing screen, the safety card appears after the scan, and Cross or Circle
   dismisses it. Confirm the card's snapshot sentence matches the files present;
   if the session snapshot failed, the card must say so.
2. Confirm L/R wraps PLUGINS -> VITALS -> ABOUT -> PLUGINS and back. Confirm the
   D-pad and the stick on ABOUT change nothing.
3. VITALS: confirm four CPU percentages, the active-core average, the
   context-switch/wakeup rates, the three memory pools, and the CPU-load trace.
   Confirm no ARM/GPU/BUS row exists. A failed read renders `--`; a frozen or
   regressed counter renders `--` for that rate only, never `0/S`. A static CPU
   reading near 100 % at idle means `idleClock` is coarser than microseconds;
   record it and treat CPU as unavailable.
4. Confirm STORAGE shows `ux0:` used/total and `uma0:` as absent or its real
   size; POWER shows percent, state, temperature, voltage, cycles, and health.
5. Confirm the rows have no dot, the green-dim active titles, the split rockers,
   the badges, and the selection wash. Confirm OFF is the left red half and ON
   is the right green half. Confirm the dock actions are pressable slabs: a
   yellow `CONFIRM & REBOOT`, dark secondary buttons with a bottom ledge, and a
   white outline on the focused action.
6. Stage an enable and a disable. Confirm `config.txt` is unchanged and no
   `.undo` exists yet; the dock shows `DRAFT - NOT WRITTEN` and the names.
   Discard and confirm the file is unchanged.
7. Stage again and `CONFIRM & REBOOT`. Confirm one `.undo` and one write, the
   reboot, and that the toggles persist after boot.
8. Stage and `SAVE, REBOOT LATER`. Confirm the file is written, the dock shows
   `SAVED - REBOOT TO APPLY`, the old plugins still run, and `REBOOT NOW`
   applies the change.
9. Preview and confirm `RESTORE ORIGINAL` and `RESTORE PREVIOUS SESSION`.
   Confirm the diff, the write, the reboot, and that `.undo` holds the
   pre-restore file. Use `UNDO LAST WRITE`; confirm the pre-batch file returns
   and the `.undo` is removed.
10. Test the battery gate below 20 % while discharging and with the power read
    invalid. Confirm the save path stays primary, the dock's `REBOOT NOW` is
    gated, ABOUT's REBOOT is dimmed with the reason, and no confirmation opens.
11. ABOUT: confirm the version reads `1.04` and the build matches the VPK. With
    no pending work, Cross opens the confirmation; Cross reboots, Circle
    cancels. Confirm the post-request notice and the failure copy.
12. Stage a batch, switch to ABOUT, and press Cross. Confirm the title is
    `REBOOT AND DISCARD`, the body says the batch will be discarded and points
    to PLUGINS, and confirming reboots without changing `config.txt`.
13. If a repair state can be produced, confirm ABOUT's REBOOT is blocked with
    `REPAIR WITH UNDO FIRST` or `RECOVERY FAILED`, not a confirmation.
14. Confirm the power-off path: hold the power button for two seconds, then
    choose Power Off. If the screen is frozen, hold for five seconds. Record
    which path the console actually shows.
15. Check `config.txt.original`, `config.txt.session`, and `config.txt.undo`;
    confirm no `.new` / `.old` leftovers and no new `boot_config.txt*` files.
16. Record the exact `WRITE FAILED <stage>` if a write fails and the cold-reset
    result. Keep the project `built` until the evidence is complete.

The remaining hardware-only claims are the target's `sceIoRename` / replace
semantics, immediate read-back visibility, the cold-reset permission for a
userland `UNSAFE` eboot on 3.65 Enso, and the dashboard/manager flow above.
Only a user-run hardware pass, with the evidence recorded here, can promote the
project to `verified`.

## 6. Failure policy

Panels degrade, they do not crash.

- A metric that could not be sampled renders `--`, never a stale or invented
  value.
- A failed sample raises `g_metrics.alert`, which draws the hazard band.
- A storage mount that does not exist (no `uma0:` without a second volume)
  is shown as absent, not as an error. A mount whose call *failed* shows the
  return code, so a permission problem is not mistaken for a missing card.
- A config write that fails leaves the original file untouched and shows
  `WRITE FAILED <stage>` inline; the draft or preview stays so the user can
  retry. A snapshot failure shows `BACKUP WARNING` but never locks the editor. An
  unrecoverable write opens the `DO NOT REBOOT` interrupt and says the config may
  be incomplete.
- A commit or restore refuses if the live file changed after the scan
  (`CONFIG CHANGED ON DISK. RESTART COCKPIT.`), and a restore that would remove
  or disable a BOOT module or locked HENKAKU path is refused in the preview with
  the path named. A no-change restore offers nothing to confirm.
- `config.txt.old` is moved back before the config is declared missing. A
  `ux0:` config that exists but cannot be read is read-only and named; the app
  never writes the shadowed `ur0:` copy in its place.
- `config.txt.undo` is taken once per batch, not once per switch. It is the
  contextual `UNDO LAST WRITE` source and is removed only after a verified undo.
  `CONFIRM & REBOOT` and `REBOOT NOW` are refused below 20 % battery unless
  charging, or when the power read is invalid.
- A config that is too large, too long, or unreadable is shown read-only with the
  reason, never rewritten from a truncated read. A path before the first section
  is a warning, not a lock.
- The app writes the active `config.txt` and its `config.txt.original` /
  `config.txt.session` / `config.txt.undo` snapshots. It writes no `.suprx`,
  `.skprx`, RCO, manifest, status file, or control file. `boot_config.txt` is
  never opened, read, copied, renamed, or written.

## 7. Roadmap

| Step | Adds | Needs |
|---|---|---|
| v1 | the plugin manager, VITALS, and ABOUT | b166 is hardware-tested on a PS Vita 1000 with 3.65 Enso; b172 adds the launcher art |
| v2 | app inventory, per-title storage | `sceAppMgrGetAppInfo`, directory walk of `ux0:app` |
| v3 | network status | `SceNetCtl` in the app process, not in a shell plugin |
| v4 | theme switching, a second visual language | a theme abstraction in `gfx/theme` |

Each step adds panels and at most one adapter. None of them change the frame
loop or the panel contract. A future clock source would be a new, separate
decision with hardware evidence; it is not planned here.
