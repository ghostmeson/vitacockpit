# Architecture

How COCKPIT is put together, what may import what, and how to extend it without
breaking the shape.

---

## 1. The dependency rule

**Source dependencies point inward, toward policy. Details depend on policy;
policy never depends on details.**

In practice, for this codebase:

- Sampling *policy* — what to read, how often, how to convert, when to raise an
  alert — must be testable without a Vita.
- The SDK and the graphics library are *details*. They live at the edge.
- A panel may read a metric and draw. A metric may not know that panels exist.

The rule is enforced by review and by the module table in §3, not by the
compiler. The one mechanical aid is that only three files may include `psp2/*`
headers (§5).

## 2. Layers

```
        ┌──────────────────────────────────────────────┐
        │  main.c          composition root            │  outermost
        ├──────────────────────────────────────────────┤
        │  app/            lifecycle, loop, layout     │
        ├──────────────────────────────────────────────┤
        │  panels/  ui/    presentation adapters       │
        ├──────────────────────────────────────────────┤
        │  gfx/            vita2d adapter              │
        │  platform/       PSP2 SDK adapter            │
        ├──────────────────────────────────────────────┤
        │  core/metrics    sampling policy, alerts     │  innermost
        │  core/history    ring container              │
        │  core/platform.h the port                    │
        └──────────────────────────────────────────────┘
                          dependencies point down
```

`core/` is the inner circle. It contains no SDK call and no draw call, so it can
be compiled and exercised on a host machine against a fake platform.

`platform/` and `gfx/` are adapters. They exist only to satisfy contracts
declared inward.

`panels/` and `ui/` compose the two. `app/` owns time and ordering. `main.c`
constructs and starts.

COCKPIT ships no `.suprx` or `.skprx`, so there is no module boundary outside
the app. That is deliberate (D99): the plugin manager was the useful half of the
old system-plugin work, and it needs no resident process, boot hook, or network
server.

## 3. Modules

| Path | Responsibility | May include |
|---|---|---|
| `core/history` | fixed-capacity ring of samples | libc |
| `core/platform.h` | the port: raw device readings, no SDK types | stdint |
| `core/metrics` | convert raw readings, maintain history, decide alerts | `core/*` |
| `core/plugins` | parse the launch config, group by load target, resolve the activated set | `core/*` |
| `core/tai_config.h` | shared taiHEN grammar and ux0/ur0 config-source resolution | `core/plugins.h`, `core/platform.h` |
| `core/plugin_editor` | byte-preserving config model, launch facts, staged working set, restore preview, one-step undo, commit | `core/*` |
| `core/input` | pad edges and repeats, analog scroll, no SDK types | libc |
| `platform/platform_psp2` | implement the port against the SDK: sample, read/write the config, list directories | `psp2/*`, `core/platform.h` |
| `gfx/layout` | screen geometry constants shared by `panels/` and `ui/` | nothing |
| `gfx/draw` | rect, batched rects, line, triangle, additive blend, clip | `vita2d.h`, `psp2/gxm.h` |
| `gfx/font` | 5×7 glyph table, text layout, glyph-rect enumeration | `gfx/draw` |
| `gfx/theme` | palette, panel chrome, meters, traces, formatters | `gfx/*`, `core/history` |
| `panels/*` | one panel each; draw one region | `core/metrics`, `gfx/theme`, `gfx/font`, `gfx/layout`, `ui/core_text` |
| `panels/plugins` | the PLUGINS header, rows, preview list, pinned dock, and its geometry | `core/plugin_editor`, `core/tai_config.h`, `gfx/*`, `ui/plugin_flow`, `ui/ui_state` |
| `panels/plugins_summary` | the HOME activated-plugin summary block | `core/plugins`, `gfx/*` |
| `ui/core_text` | pure CORE readout formatting, including the per-field `--` contract | `gfx/theme` |
| `ui/header` | wordmark, clock, tabs, hazard band | `core/metrics`, `gfx/theme`, `gfx/font`, `gfx/layout`, `ui/tabbar` |
| `ui/ui_state` | navigation state: tab, two scrolls, selection, dock focus, dock message, and the one modal | nothing |
| `ui/tabbar` | tab labels | `gfx/*`, `ui/ui_state` |
| `ui/modal` | the error interrupt | `gfx/*`, `ui/ui_state` |
| `ui/plugin_flow` | the derived dock state, action order, labels, availability, warnings, focus, and one dock-fact table; no SDK, renderer, or modal dependency | `core/plugin_editor`, `core/metrics`, `ui/ui_state` |
| `ui/plugin_nav` | tab/list/dock/modal event routing, SAVE-vs-CONFIRM commit mode, and scroll clamps; no SDK or renderer dependency | `core/input`, `ui/plugin_flow`, `ui/ui_state` |
| `app/screen` | one frame: header, active tab, HOME frame strip, modal, sweep | `gfx/*`, `panels/*`, `ui/*`, `ui/ui_state` |
| `app/app` | lifecycle, fixed-rate scheduler, frame loop, pad adapter, nav-action execution, transaction dispatch, `UiState` | everything below |
| `main.c` | `vita2d_init()`, `app_run()`, `vita2d_fini()` | `app/app`, `vita2d.h` |

`app/screen.c` exists so the host preview renders the shipped composition
instead of a copy of it (D26). It makes no SDK call and reads no clock, which is
what lets `tools/preview` link it unchanged.

`core/plugins` and `core/plugin_editor` deliberately keep separate line models:
the first only needs the activated set, the second needs offsets, line endings,
reachability, and pending state. They share `core/tai_config.h` for the grammar,
so a section name or plugin suffix cannot fork between HOME and PLUGINS.

`ui/plugin_flow` is policy, not presentation: it owns the derived
`PluginFlowState`, the action order and labels, availability, warning precedence,
and the focus transitions, but it draws nothing and calls no SDK. `ui/plugin_nav`
owns what an input event means for the tab, the list, the dock, and the modal,
plus the SAVE-vs-CONFIRM commit mode and the scroll clamps; `app/app` only
executes the result. Both are platform-free, which is what lets `tools/test`
link them and exercise the routing without a Vita. `ui/modal` draws the one
error interrupt; there are no choice modals and no self-plugin block.

`core/plugin_editor` is the only writer of `g_editor`. A stage changes the
working text and reparses it with no platform call; a commit is the only path
that writes the live file and the only path that replaces `.undo`. Both commits
share one `commit_text` protocol: the stale check, the optional `.undo`, the
verified write, the failure mapping, and the baseline adoption. The preview's
saved-work and removed-path scratch and the launch/file-row cache are file-private
in `plugin_editor.c`; the header exposes only the counts, the query accessors,
and the launch lookup the host test needs. The panel and the dock read the cached
flags and never open, stat, or write a file. A restore is refused when the live
file was not fully parsed, and the `write_unrecovered` state keeps undo available
to repair it (D79).

Outside the app: `tools/fake/` is the shared mutable port double, `tools/test/`
links it against the real `core/` to assert the sampling policy and the plugin
transaction, and `tools/preview/` renders the real screen through a software
vita2d shim. None of them is linked into `cockpit.self`.

One rule the type system cannot express: **every vertex buffer handed to
`vita2d_draw_array` must come from `vita2d_pool_memalign`.** GXM reads the vertex
stream when the scene is submitted, so the buffer has to be GPU-mapped and alive
until the frame ends. A stack or static array faults the GPU on the first frame;
that is exactly what the first hardware build did (D29).

No cycles. `core` never includes `gfx`, `panels`, `ui`, or `app`.

## 4. The port

`core/platform.h` declares what the machine can report, in plain C types. No
`Sce*` type crosses it.

```c
typedef struct {
    uint64_t free_bytes;
    uint64_t total_bytes;
    int ok;
} PlatformStorage;

void platform_read_storage(int index, PlatformStorage *out);
void platform_read_memory(PlatformMemory *out);
void platform_read_cpu(PlatformCpu *out);
void platform_read_power(PlatformPower *out);
void platform_read_identity(PlatformIdentity *out);
void platform_read_clock(PlatformClock *out);
double platform_uptime_seconds(void);

int platform_file_exists(const char *path);

typedef enum {
    PLATFORM_READ_ABSENT = 0,
    PLATFORM_READ_OK,
    PLATFORM_READ_TOO_LARGE,
    PLATFORM_READ_UNREADABLE,
} PlatformReadStatus;

PlatformReadStatus platform_read_config(const char *path, char *buf, int cap,
                                        int *out_len);
int platform_write_config(const char *path, const char *text, int len);
const char *platform_write_error(void);
int platform_backup_config(const char *path, const char *text, int len,
                           int *original_ok, int *session_ok);
int platform_recover_swap(const char *path);

typedef enum {
    PLATFORM_BACKUP_ORIGINAL = 0,
    PLATFORM_BACKUP_SESSION,
    PLATFORM_BACKUP_UNDO,
} PlatformBackup;

PlatformReadStatus platform_read_backup(const char *path, PlatformBackup kind,
                                        char *buf, int cap, int *out_len);
int platform_snapshot_undo(const char *path, const char *text, int len);
int platform_request_reboot(void);

typedef void (*PlatformDirCallback)(const char *name, int is_dir, void *user);
int platform_list_dir(const char *dirname, PlatformDirCallback cb, void *user);
```

There is no clock reading in the port. The old `PlatformClocks` and the
app-local `.suprx` were removed together (D99): without a kernel or bridge
source, the userland request is not the applied value, and the project does not
render a number it cannot stand behind. CORE reports CPU load and memory; the
CPU reading comes from `platform_read_cpu`, which returns the userland
scheduler counters, not a clock (D103). `core/metrics` publishes a cumulative
rate only when its total advances; a zero, frozen, or regressed counter renders
`--` for that field. `ui/core_text` owns that formatting, so the per-core and
caption absence paths are host-tested without a panel.

The config calls stay in the same adapter because every SDK file operation
belongs to one module (D7). `platform_read_config` opens first and distinguishes
absent, empty, too-large, and unreadable; a short read is unreadable.
`platform_write_config` writes a verified sibling, moves the original aside,
moves the sibling into place, and reads the live file back; `platform_write_error`
names the stage that failed so a console report is actionable. `platform_backup_config`
writes `config.txt.original` once and `config.txt.session` every launch.
`platform_read_backup` exposes the three snapshots to the editor's preview: scan
caches `.original` and `.session` availability, and reads `.undo` only to derive
whether it differs from the live file. Scan may read `.undo` for that fact and
must never rewrite, rotate, or remove it. `platform_snapshot_undo` writes the
one-step `config.txt.undo` once per batch; `platform_remove_undo` removes it only
after a verified undo. `platform_recover_swap` repairs a live file left absent by
an interrupted swap. `platform_request_reboot` is the only SDK call that can
cold-reset the console; `platform_list_dir` feeds the orphan section without
exposing `SceUID` or `SceIoDirent`.

`ur0:tai/boot_config.txt` is deliberately outside the port. The app never opens
it; the three well-known boot module names are locked by name (D61).

Why a port rather than calling the SDK from `core/metrics`:

1. **No console is available.** The sampling policy — unit conversion, alert
   thresholds, history cadence — is the part most likely to contain a bug, and
   it is the part that can be tested off-device. A port is what makes that
   possible.
2. **Several platform claims are `[C]`.** The devctl storage unit, the target's
   rename/replace and immediate read-back semantics, the snapshot reads, and the
   cold-reset permission are unconfirmed on hardware. Keeping them in one
   adapter file means a correction is a one-file change with no reach into
   policy.

This is a *partial* boundary, deliberately: plain functions, not an interface
struct, because there is exactly one implementation and C has no interfaces. A
test double is produced by linking `platform_fake.c` instead of
`platform_psp2.c`, not by swapping a vtable at runtime.

That is not hypothetical: `tools/preview/` does exactly that. It links the real
`core`, `gfx`, `panels`, and `ui` against `tools/fake/fake_platform.c` and a
software stand-in for the four vita2d calls `gfx/draw.c` makes, then writes PNGs.
It is how the layout was checked before any hardware existed. It is a developer
tool, never linked into `cockpit.self`.

## 5. Where the SDK may appear

Exactly two **userland app** files include `psp2/*` headers:

| File | Why |
|---|---|
| `platform/platform_psp2.c` | all device sampling and config I/O |
| `app/app.c` | the process clock and the pad |

The renderer boundary is separate: `gfx/draw.c` and `main.c` include `vita2d.h`
(the drawing adapter and the composition root). They do not include `psp2/*`.

Any other userland file that reaches for the SDK is a layering bug. If a panel
needs a new value, add it to the port and to `Metrics` — do not call the SDK from
the panel.

## 6. Data flow

```
  platform_psp2          core/metrics              panels
  ─────────────          ────────────              ──────
  sceKernel*           ─┐
  sceIoDevctl          ─┼─► platform_read_* ─► convert ─► g_metrics ─► draw
  sceRtc*              ─┤        (10 Hz)        units       │
  sceAppMgr*           ─┘                                  │
                                                      core/history
  process time ──►  metrics_sample_frame()  ────────────┘
                       (every frame)
```

Panels never call the SDK and never mutate metrics. They read `g_metrics` and
draw. Sampling is the only writer.

### Cadence

| Rate | What | Why |
|---|---|---|
| every frame | frame time, FPS (EMA, α = 0.05) | needs frame resolution |
| 10 Hz | memory, CPU, storage, power, identity, clock | syscalls are not free; these change slowly |

10 Hz × 200 samples = a 20-second trace.

### Editor and input

```
pad ─► core/input ─► app/app ─► ui/plugin_flow ─► app/app ─► core/plugin_editor
       (events,     (adapter,   (dock state,      (command)   (working text,
        scroll_y)    dispatch)   focus, copy)                  preview, commit)
                                                                   │
                                                                   ▼
                                                        platform/platform_psp2
                                                        (read, snapshot, write,
                                                         verify, cold reset)
```

`core/input` turns the pad into at most one edge/repeat event per frame plus a
continuous `scroll_y`; it knows no SDK type. `app/app` is the adapter: it applies
the stick scroll to the active surface and maps the focused dock command to the
editor. `ui/plugin_flow` derives the dock state and focus from the editor and the
metrics sample, but owns no file and draws nothing. `core/plugin_editor` is the
only writer of `g_editor`; a stage never leaves memory, and only a commit or a
preview/undo commit calls the platform write path. The PLUGINS panel reads the
cached facts and draws; it never reads, stats, or writes a file.

## 7. Platform claims and their status

`[V]` verified against a primary source. `[C]` community-reported, unconfirmed.

| Claim | Status | Where |
|---|---|---|
| `sceIOGetstat` fills `st_mode` with the `SCE_S_*` family and `st_attr` with `SCE_SO_*` | `[V]` `psp2common/kernel/iofilemgr.h`; `SCE_SO_ISDIR(st_mode)` is false for every real directory | `platform_psp2.c` |
| SceNet is not loaded in SceShell at boot | `[V]` a `*main` probe returned `sceSysmoduleIsLoaded(SCE_SYSMODULE_NET) == 0` on this 3.65 Enso console (2026-11-05) | historical (D99) |
| Loading SceNet and then another `*main` module from a `*main` plugin is fatal to SceShell | `[V]` `C1-14738-3` on this console after the diagnostic loader ran; the loader is withdrawn (D99) | historical (D99) |
| An unsigned app-local `.suprx` loads from `app0:` and resolves a kernel-exported user library | `[C]` the old bridge built and shipped, but never ran successfully on hardware (D85, retired by D99) | historical |
| Free memory returns bytes across three pools | `[V]` `psp2/kernel/sysmem.h` | `platform_psp2.c` |
| Battery percent/temp/voltage/cycles/health are userland | `[V]` `psp2/power.h` | `platform_psp2.c` |
| Firmware string via `sceKernelGetSystemSwVersion` | `[V]` `psp2common/kernel/modulemgr.h` | `platform_psp2.c` |
| `SceIoDevInfo` layout, 24 bytes | `[V]` `psp2common/kernel/iofilemgr.h` | `platform_psp2.c` |
| `sceIoDevctl("ux0:", 0x3001, …)` is the device-info command | `[V]` `psp2/io/devctl.h` demonstrates it verbatim | `platform_psp2.c` |
| Its results are byte counts | `[C]` the unit is documented nowhere | `platform_psp2.c` |
| Model codes `SCE_KERNEL_MODEL_VITA` / `SCE_KERNEL_MODEL_VITATV` | `[V]` `psp2common/kernel/sysmem.h` | `platform_psp2.c` |
| `sceKernelGetModel()` cannot separate PCH-1000 from PCH-2000 | `[V]` one code for both | `platform_psp2.c` |
| `sceIoRename` to an absent target | `[C]` the fake exercises the transaction; the SDK call is not run on hardware | `platform_psp2.c` |
| Replacing the live config without rename-over | `[C]` the rename-away swap is implemented; the target's rename semantics are unverified | `platform_psp2.c` |
| Immediate read-back after the swap sees the new bytes | `[C]` the adapter verifies by reading the live file; the fake models it, hardware does not yet | `platform_psp2.c` |
| Reading `.original`, `.session`, and `.undo` through the port | `[C]` the host fake reads them; the target's SDK reads are not exercised on hardware | `platform_psp2.c` |
| `scePowerRequestColdReset` exists | `[V]` `psp2/power.h`; linked from `ScePower` | `platform_psp2.c` |
| `sceKernelGetSystemInfo` is a userland `SceLibKernel` export with per-core idle, wakeup, and thread-switch counters | `[V]` `psp2/kernel/threadmgr/thread.h` | `platform_psp2.c` |
| `cpuInfo[].idleClock` advances in microseconds | `[C]` the header does not state the unit; `core/metrics.c` rejects a delta larger than elapsed time. A coarser unit is not detectable from the counter, so the hardware checklist treats a static 100 % at idle as unavailable. | `core/metrics.c`, `platform_psp2.c` |
| A userland UNSAFE eboot may cold-reset on 3.65 Enso | `[C]` the header says it always returns 0; the reset itself is unverified | `platform_psp2.c` |
| `sceIoSyncByFd` flushes data before close | `[V]` declared in `psp2/io/fcntl.h`; its effect is best-effort here | `platform_psp2.c` |
| `boot_config.txt` is never opened | `[V]` no source path names it | `core/plugin_editor.c` |


The file, power, and CPU claims remain `[C]`: the devctl byte unit, the target's
`sceIoRename`/replace semantics, immediate read-back visibility, reading the
three snapshots, whether a userland UNSAFE eboot may cold-reset, how completely
`sceIoSyncByFd` flushes, and the idle-clock unit on 3.65. The read path no longer depends on
`sceIoGetstat` succeeding before `sceIoOpen`, and the normal write path no longer
renames over an existing file.

If a `[C]` claim is corrected on hardware, correct it in the file its Where
column names.

## 8. Panel contract

```c
typedef struct Panel {
    const char *title;
    uint32_t accent;
    void (*draw)(const struct Panel *self, Rect r);
} Panel;

typedef struct {
    const Panel *panel;
    Rect rect;
    PanelChrome chrome;
} PanelSlot;

extern PanelSlot g_layout[];
extern const int g_layout_count;

/* Height of the home canvas, derived from the lowest panel edge. */
int layout_canvas_height(void);

/* Largest valid home scroll offset; zero when the two rows fit. */
int layout_max_scroll(void);
```

`app_run` owns one `UiState` and passes a read-only pointer to `screen_draw`.
On HOME, `screen_draw` walks `g_layout`, draws the chrome through
`theme_panel()`, then calls the panel's `draw` with its rect. On PLUGINS, it calls
`plugins_screen_draw(ui, plugins_content_height())`, which owns its own canvas,
selection, clipping, and pinned dock. Panels own no data: the only mutable state
is `UiState` in `app/app`, the launch/edited config models in `core/plugins` and
`core/plugin_editor`, and the derived dock policy in `ui/plugin_flow` (pure
functions over the editor and the metrics sample). `core/plugins.c` owns
`g_plugins`, and `core/plugin_editor.c` owns `g_editor`; no panel writes either.

`UiState` carries only navigation and the inline dock cursor: `tab`,
`home_scroll`, `plugins_scroll`, `selection`, `focus`, `dock_choice`,
`dock_message`, and the one error modal. The dock state itself is derived every
frame by `plugin_flow_state()`; it is never stored, so it cannot drift from the
editor.

There is no elapsed-time argument to a panel. Nothing inside a panel animates:
the traces are driven by the history they read, and the only time-based motion
is the screen-wide sweep, which `app_run` owns. An unused parameter would be
dead code (rule 9).

Geometry lives in the layout table, not in the panel. That is what allows an
asymmetric screen (D17): a panel that computed its own rect from a grid index
could not be 552 wide while its neighbour is 360.

The home table is fixed. It holds no plugin slot: the ledger moved to its own
tab, and `plugins_screen_draw(ui, content_h)` owns that canvas, its row
geometry, its selection, and its dock (D54, D57). Each `PluginInfo` carries its
`PluginSection`; a file listed under several targets keeps the most privileged
one. The launch list holds up to `PLUGINS_MAX` entries and sets
`PluginList.truncated` when a config exceeds that (D50). The editable model in
`core/plugin_editor` has its own ceilings and reports its own truncation.
`layout_canvas_height()` and `layout_max_scroll()` are derived from the home
table, so neither can drift from it (D49).

`screen_draw()` skips any home panel whose translated rect falls entirely
outside the view band, and `plugins_screen_draw()` applies the same test per
group and row. The scissor would discard every pixel of them anyway.

Panels do not know whether they are the hero or a satellite. The frame drawing
that distinguishes them is `theme_panel()`'s job, selected by `PanelSlot.chrome`
— so a panel can be promoted by editing the layout table alone.

### Adding a panel or a tab

1. For a HOME panel, create `src/panels/<name>.c`, define `Panel panel_<name>`,
   and add one `PanelSlot` entry in `panels/registry.c`. There is no mutable
   layout pass; the rect is fixed.
2. For a tab, add a `UiTab` value, a tab label in `ui/tabbar.c`, and a draw
   branch in `app/screen.c`. Keep the tab's state in `UiState`; do not add a
   second global. Tabs that need their own canvas, like PLUGINS, own their
   geometry and clipping in their panel module.
3. Add the source file to `CMakeLists.txt` and to `tools/preview/run.sh`. Both
   list sources explicitly; the preview list is the one that gets forgotten, and
   forgetting it means the preview silently stops rendering the panel.
4. If it needs a value that `Metrics` does not carry, add it to the port and to
   `Metrics` — see below.

Adding a panel means the layout must absorb it. Because the screen is
asymmetric, that is a deliberate edit to the rect table, not an automatic
reflow — decide which region gives up space.

Nothing else changes. The loop and the header are untouched; the two build
lists from step 3 are the only mechanical edit.

### Adding a metric

1. Add the field to `Metrics` in `core/metrics.h`, **and a matching
   `<field>_valid` flag**. Every metric carries one: it is how a panel knows to
   render `--` instead of the zero it happens to hold.
2. Add a `platform_read_*` function to `core/platform.h`, with an `ok` field.
3. Implement it in `platform/platform_psp2.c`, with a `[V]`/`[C]` comment.
4. Call it from `metrics_sample_slow()` and set the valid flag from `ok`.
5. Record any new platform claim in §7 and in `docs/DECISIONS.md`.
6. Add a case to `tools/test/test_metrics.c` — at minimum one that fails the
   read and asserts the panel would render absence.

## 9. Deliberate deviations

Recorded so a future reader does not "fix" them by accident.

- **No dependency-injection container or runtime polymorphism** beyond one
  function pointer in `Panel`. The system has one platform and one theme; a
  container would be ceremony.
- **The port is functions, not an interface struct.** One implementation, C, no
  interfaces. Test doubles come from the link line.
- **`gfx/draw.c` includes `psp2/gxm.h`.** `gfx` is the rendering adapter, so a
  GXM enum is inside its remit; pulling it up into `core` would be worse.
- **`g_metrics` is a global.** One screen, one snapshot, one writer. Passing a
  500-byte struct through every panel would add noise without adding safety.
- **Layout is compile-time.** No responsive system. The Vita has exactly one
  screen size (D13).
- **No plugin payload, ever.** The app is the product. A shell hook, FTP worker,
  or app-local bridge is not a feature toggle; it is a second failure domain
  with no manager value (D99).
