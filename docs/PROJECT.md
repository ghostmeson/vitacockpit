# Project

What COCKPIT is, what it is not, and where it stands.

---

## 1. Purpose

A live readout of the machine it runs on, plus one screen where the taiHEN
plugin list can be changed. It is an instrument panel first: every number it
shows is real, and the plugin manager exists because a readout you cannot act
on becomes an inventory.

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
| HOME header | wordmark, clock, HOME/PLUGINS tabs |
| CORE | per-core CPU load, active-core average, context-switch/wakeup rates, 20 s CPU-load trace; free user / CDRAM / physical-contiguous pools |
| STORAGE | `ux0:`, `uma0:` used and total, with the unit chosen from each volume's size (GB or MB), segmented meters |
| POWER | battery percent, charging state, temperature, voltage, cycles, health |
| SYSTEM | firmware string, app run time, device model |
| PLUGINS summary | a full-width block below both columns: loaded count, group counts, and a config warning |
| PLUGINS tab | the full config ledger with a staged working set, an inline transaction dock, launch-set and next-boot badges, split rockers, the two restore previews, contextual last-write undo, and the config snapshots |
| FRAME | measured FPS, frame time, 20 s trace |

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
- A third tab, plugin presets, a line-level diff, or a draft that survives a
  restart. The restore preview is plugin-level and replaces the whole file.

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
- No hardware is reachable from the current development environment. The b13
  run in §5 is the only dashboard evidence; b156 has not run on the console.
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
HOME the FRAME strip is pinned too, while PLUGINS uses the bottom of the screen
for its transaction dock. A thin bar on the right shows position. Press L or R to
switch between HOME and PLUGINS. The HOME `PLUGINS` block is read-only: it counts
the launch set, and it sits below the fold, so scroll down to reach it. Pending
edits are shown only on the PLUGINS tab that made them.

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
build/cockpit-v<version>-b<build>.vpk        e.g. cockpit-v01.00-b156.vpk
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
| Builds and packages | `[V]` clean default and safe builds, 0 compiler warnings; `cockpit-v01.00-b156.vpk` is 132,734 bytes, SHA-256 `84c9d16c8c10d5c8ad918ff5a5177041780884e93829e3a1b5cf991ff0b6335c`; the VPK contains `eboot.bin` and `sce_sys/` only; the unstripped ELF is kept as `build/cockpit` |
| Host tests (sampling, plugin model, draft/commit, restore, undo, input, dock flow) | `[V]` `tools/test/run.sh`, 161 metrics + 1,208 editor + 43 UI checks, 0 failures |
| Plugin list (`ux0:` first, `ur0:` fallback) | `[V]` host harness keeps a 19-entry real config in full and groups it as KERNEL 9 / SYSTEM 8 / SYSTEM APPS 1 / ALL TITLES 1; host tests cover BOM, commented entries, long names, section mapping, priority merge, the 64-entry ceiling, full-path duplicate rows, `*!` halts, lock/warn policy, the two snapshots, and the exact `#` edit |
| Staged transaction and restore | `[V]` host tests cover stage/discard/commit, one `.undo` per batch, the stale-file refusal, the plugin-level restore diff, `WILL REMOVE`, the locked-drop and no-change refusals, the read-only refusal for an unreadable or truncated live config, the `*!` halt before a protected path, the undo repair after an unrecovered write, the removed-path pending truth, the failed-save `.undo` preservation, the saved/last-write truth table, focus and the battery gate. `[C]` the end-to-end flow is unverified on hardware. |
| Memory, CPU, storage, power, identity APIs declared in SDK | `[V]` header inspection; `sceKernelGetSystemInfo` is a userland `SceLibKernel` export |
| CPU idle-clock unit and 3.65 availability | `[C]` `SceKernelSystemInfo.cpuInfo[].idleClock` is treated as microseconds. A delta larger than elapsed time is refused; a coarser unit is not detectable from the counters, so a static 100 % at idle is treated as unavailable. The call has not run on this console. |
| `SceIoDevInfo` layout | `[V]` `psp2common/kernel/iofilemgr.h`, 24 bytes |
| `sceIoDevctl("ux0:", 0x3001, …)` command | `[V]` `psp2/io/devctl.h` demonstrates it verbatim |
| Storage result unit (bytes) | `[C]` — the devctl byte unit remains unconfirmed |
| Device model codes | `[V]` `psp2common/kernel/sysmem.h` |
| Screen layout and typography | `[V]` host preview render, 40 states, busiest 5,429 draw calls against the 8,000 budget; header slots, rows, preview list, every dock state with its pressable button model, the HOME-only FRAME strip, the off-left/on-right rocker, the 12 px dock padding, and CORE/POWER outlines (D53, D54, D77, D78, D81, D83, D99) |
| Renders on hardware | `[V]` 2026-11-02 (b13): text, memory, power, identity and frame all draw on the console. 2026-11-04 (b124): the app and the PLUGINS screen draw. 2026-11-08: the user reports b150 runs on the console; CORE was memory-only. b156 adds the CPU readout and the review fixes and has not run on hardware yet. |
| b74 startup crash | `[V]` C2-12828-1; the core names `COCKPIT01`, stop reason `0x30003`, PC `0`, and r0 `0x812c0ae4` (`&reading` at `platform_read_clocks+0x2e`). The weak import and the whole clock path are gone (D99). |
| Config write path | `[C]` the adapter's rename-away swap and live read-back are implemented and build; host tests exercise the same transaction in the fake, not the SDK calls, and the target's rename semantics are unverified. A failure names the stage (`OPEN`, `DATA`, `CLOSE`, `VERIFY`, `MOVE`, `RENAME`, `READBACK`, `RESTORE`, `UNDO`). |
| Restore reads | `[V]` host tests read `.original`, `.session`, and `.undo` through the same port; `[C]` the SDK file reads are not exercised on hardware. |
| Cold reset | `[V]` `scePowerRequestColdReset` is declared in `psp2/power.h`; `[C]` that an UNSAFE userland eboot may perform it on 3.65 Enso |
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
policy, the navigation routing and SAVE/CONFIRM mapping, and the input reducer.
The host preview (`tools/preview/run.sh`) renders the shipped composition against
the same fake, 40 states with the busiest at 5,429 draw calls. Neither proves
anything about the device: every reading in them is synthetic.

### Hardware handoff

The next action is to flash `build/cockpit-v01.00-b156.vpk` and run this
checklist on the 3.65 Enso console. The project stays `built` until the user
records the evidence; no item below is claimed by the host suite.

1. Flash the VPK and launch COCKPIT. Confirm the header reads `USING`, the
   counts, and the prompt; no `ROWS` / `LOADED` / `CROSS CHANGE` text remains.
   Confirm FRAME is visible on HOME and absent on PLUGINS.
2. Confirm CORE shows four CPU percentages, the active-core average, the
   context-switch/wakeup rates, the three memory pools, and the CPU-load trace.
   Confirm no ARM/GPU/BUS row exists. A failed CPU or memory read leaves its
   trace alone and renders `--`; a frozen or regressed cumulative counter renders
   `--` for that rate only, never `0/S`. A static CPU reading near 100 % at idle
   means `idleClock` is coarser than microseconds; record it and treat CPU as
   unavailable. If the readout moves with load, the unit assumption is usable.
3. Confirm STORAGE shows `ux0:` used/total and `uma0:` as absent or its real
   size; POWER shows percent, state, temperature, voltage, cycles, and health.
4. Confirm the rows have no dot, the green-dim active titles, the split rockers,
   the badges, and the selection wash. Confirm OFF is the left red half and ON
   is the right green half. Confirm the dock actions are pressable slabs: a
   yellow `CONFIRM & REBOOT`, dark secondary buttons with a bottom ledge, and a
   white outline on the focused action. Confirm the dock's top and bottom inner
   padding look even.
5. Stage an enable and a disable. Confirm `config.txt` is unchanged and no
   `.undo` exists yet; the dock shows `DRAFT - NOT WRITTEN` and the names.
6. Discard. Confirm the file is unchanged and the dock returns to idle or the
   saved state.
7. Stage again and `CONFIRM & REBOOT`. Confirm one `.undo` and one write, the
   reboot, and that the toggles persist after boot.
8. Stage and `SAVE, REBOOT LATER`. Confirm the file is written, the dock shows
   `SAVED - REBOOT TO APPLY`, the old plugins still run, and `REBOOT NOW`
   applies the change.
9. Preview and confirm `RESTORE ORIGINAL` and `RESTORE PREVIOUS SESSION`.
   Confirm the diff, the write, the reboot, and that `.undo` holds the
   pre-restore file.
10. Use `UNDO LAST WRITE`. Confirm the pre-batch file is restored and the `.undo`
    is removed. If the dock still offers it after a reboot, that is the one-step
    contextual undo; the app cannot distinguish a relaunch from a reboot.
11. Test the battery gate below 20 % while not charging and with the power read
    invalid. Confirm the save path is primary and `REBOOT NOW` is gated.
12. Exercise the sticks and D-pad: sticks scroll the list and the preview, D-pad
    selects, L/R switch tabs, Down enters the dock, Left/Right skips unavailable
    actions, Circle returns.
13. Check `config.txt.original`, `config.txt.session`, and `config.txt.undo`;
    confirm no `.new` / `.old` leftovers and no new `boot_config.txt*` files.
14. Record the exact `WRITE FAILED <stage>` if a write fails and the cold-reset
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
| v1 | the dashboard and the plugin manager | b156 builds and is host-tested; the hardware checklist above is pending |
| v2 | app inventory, per-title storage | `sceAppMgrGetAppInfo`, directory walk of `ux0:app` |
| v3 | network status | `SceNetCtl` in the app process, not in a shell plugin |
| v4 | theme switching, a second visual language | a theme abstraction in `gfx/theme` |

Each step adds panels and at most one adapter. None of them change the frame
loop or the panel contract. A future clock source would be a new, separate
decision with hardware evidence; it is not planned here.
