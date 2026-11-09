# Decision log

Architecture and design decisions, in the order they were made. Each entry
records what forced the choice and what it costs, so a future reader can tell a
deliberate trade from an oversight.

**Status values:** Accepted · Superseded by D<n> · Proposed

---

## D1 — Panels are a registry, not a switch

**Status:** Accepted
**Context:** COCKPIT must grow into a full dashboard. A single `draw_screen()`
function would grow without bound and every panel would be able to reach into
every other.
**Decision:** A panel is data — a `Panel` struct with a title, an accent, and one
draw function — collected in `g_layout[]`.
**Consequence:** Adding a panel is one file and one registry line. Costs one
indirection per panel per frame, which is unmeasurable at five regions.
**Rejected:** a `switch` in the loop (no isolation), and a virtual filesystem of
plugins (absurd for this size).

## D2 — A hand-built 5×7 bitmap font

**Status:** Accepted
**Context:** VitaSDK 2026.08 ships no font file. Bundling a TTF means a licence,
~200 KB, and a freetype path at runtime.
**Decision:** A glyph table in `gfx/font.c`, with sizes expressed as integer
font-pixel scale.
**Consequence:** ~300 bytes of glyph data, zero dependencies, and crisp block
type that suits additive glow. Costs: no kerning, Latin only, new glyphs added by
hand.
**Rejected:** the system PGF (proportional, wrong voice), a bundled TTF (weight
and licence for no gain at HUD sizes).

## D3 — Glow by additive blending, not shaders

**Status:** Accepted (revised)
**Context:** vita2d exposes no custom shader path.
**Decision:** `vita2d_set_blend_mode_add(1)`, exposed as
`draw_set_additive()`. A shape that should glow is drawn more than once at
falling intensity while the mode is on.
**Consequence:** Real bloom from a handful of extra draw calls, with no shader,
no render target, and no post-process pass.

Revised from "expanding translucent rects", which described a helper nothing
ended up calling. The only glowing shape is the wordmark, and it builds its halo
from four offset additive passes of its own glyphs; a generic `draw_glow_rect`
was removed rather than kept unused (rule 9). The falloff is stepped rather than
smooth, which is invisible at this scale.

## D4 — Cyberpunk 2077's game HUD, not the anime

**Status:** Accepted
**Context:** Both were named as references. They are different visual systems:
the game HUD is restrained, the anime is bloom-heavy.
**Decision:** Follow the game HUD.
**Consequence:** Legibility survives a 5-inch screen. The anime's bloom would
have made small labels unreadable.

## D5 — Yellow leads; red is reserved

**Status:** Superseded by D40; red returns as a narrow off-state token in D78
**Context:** A HUD with several saturated colours has no way to signal urgency.
**Decision:** Yellow is the only saturated colour used at rest. Red means the
machine needs attention.
**Consequence:** A red screen is unambiguous. Costs colour variety.

## D6 — No CPU load percentage

**Status:** Superseded by D103 (2026-11-08); the premise that no userland API
reports system CPU utilisation was wrong.
**Context:** No userland API reports system-wide CPU utilisation. `psp2/power.h`
exposes clock *frequency*, not load.
**Decision:** Show clock frequency and the app's own frame time. Do not
synthesise a percentage.
**Consequence:** Every number is real. A fabricated "CPU 47 %" would discredit
the rest of the panel.
**Rejected:** deriving load from idle-time sampling, which would measure this
process, not the system.

## D7 — Platform access behind a port

**Status:** Accepted
**Context:** The sampling policy — unit conversion, alert thresholds, history
cadence — is the part most likely to contain a bug, and no Vita is available to
test it on. Calling the SDK from `core/metrics.c` would make that policy
untestable.
**Decision:** `core/platform.h` declares raw readings in plain C types; the SDK
call lives in `platform/platform_psp2.c`.
**Consequence:** The policy compiles and runs against a fake platform on the
host. Costs one indirection and one extra file.
**Rejected:** full four-circle clean architecture with interactors and presenters
— ceremony for a 1,500-line single-screen app. This is a partial boundary by
design (see [ARCHITECTURE.md §4](ARCHITECTURE.md#4-the-port)).

## D8 — `[C]` platform quirks live in one file

**Status:** Accepted
**Context:** `SceIoDevInfo`, the device model constants, and the storage unit are
all absent from or unconfirmed by the installed headers. They are community
knowledge.
**Decision:** All three are declared in `platform/platform_psp2.c` with `[C]`
comments, and nowhere else.
**Consequence:** Correcting any of them after hardware testing is a one-file
change that cannot reach into policy or presentation.

## D9 — Safe eboot

**Status:** Accepted
**Context:** Nothing in COCKPIT needs private or system APIs.
**Decision:** Default `vita_create_self`, authid `0x2F00000000000002`.
**Consequence:** No "Enable Unsafe Homebrew" requirement and no install warning.
**Rejected:** `UNSAFE` for convenience, which would relax the whole system for no
gain.

## D10 — Two sampling cadences

**Status:** Accepted
**Context:** Frame time needs frame resolution. Storage and battery syscalls do
not, and are not free.
**Decision:** Frame metrics every frame; everything else at 10 Hz.
**Consequence:** Slow syscalls stay out of the frame path. Costs up to 100 ms of
lag on the wall clock, which is invisible.

## D11 — Layout is a compile-time rect table

**Status:** Accepted (revised; the tab composer is D54)
**Context:** One screen, one resolution, five regions of unequal size.
**Decision:** `SCREEN_W`, `SCREEN_H`, `MARGIN`, `GAP` in `gfx/layout.h`, and an
explicit `PanelSlot` table in `panels/registry.c` giving each panel its rect.
**Consequence:** The layout is one readable table and can be asymmetric. Adding a
region means deciding which other region gives up space; there is no reflow.
**Superseded:** an earlier version computed rects from a column/row index, which
could only ever produce equal cells.

## D12 — No persistence in v1

**Status:** Superseded by D54-D66 (the plugin manager writes the active config
and its two snapshots; the metrics history is still session-only)
**Context:** The app measures; it does not record.
**Decision:** COCKPIT writes nothing to disk.
**Consequence:** It cannot corrupt a memory card, needs no save-data setup, and
history resets on launch. Costs the ability to show long-term trends.

## D13 — Fixed 960×544, no scaling

**Status:** Accepted
**Context:** The Vita has exactly one screen size.
**Decision:** Draw at native resolution with no virtual canvas.
**Consequence:** Pixel-exact HUD. A future resolution change is a layout edit,
not a config value.

## D14 — History capacity 200 samples

**Status:** Accepted
**Context:** The trace should show enough history to be useful without holding a
meaningful amount of memory.
**Decision:** `HISTORY_CAP = 200`, pushed at 10 Hz.
**Consequence:** A 20-second window in 800 bytes per trace. Costs nothing.

## D15 — The hazard band is an alert channel

**Status:** Accepted
**Context:** Yellow/black hazard stripes are a Cyberpunk signature and the
obvious thing to put under the header as ornament.
**Decision:** It renders only when a real condition holds — battery under 20 %
while discharging, a failed memory sample, or `ux0:` not answering.
**Consequence:** A band on screen means something. On a healthy console the
screen is quiet. This is the difference between a signal and wallpaper.

## D16 — `g_metrics` is a global with one writer

**Status:** Accepted
**Context:** One screen, one snapshot, one sampler.
**Decision:** A single `Metrics g_metrics` is written only by `core/metrics.c`
and read by panels.
**Consequence:** No plumbing of a 500-byte struct through every call. Costs
global mutable state, which is acceptable while there is exactly one writer and
one consumer set. If COCKPIT ever gains a second screen or a background sampler,
this becomes a bug and must be revisited.

## D17 — Asymmetric layout: hero, satellites, status strip

**Status:** Accepted
**Context:** The first layout was six identical panels in a 3 × 2 grid. That is
the default shape of every monitoring dashboard, and it encodes nothing about the
machine — clocks, storage, and a frame counter all received the same 300 × 220.
**Decision:** Three tiers. CORE is the hero at 552 × 240 (210 since D48) and carries clocks and
memory together. STORAGE, POWER, and SYSTEM are satellites sized by how much they
hold. FRAME is a 44 px full-width status strip, because it measures the app
rather than the machine and should not compete with it.
**Consequence:** Area tracks importance and the screen reads as a HUD over a
machine rather than a grid of cards. Costs: the layout cannot be derived from a
grid index (D11), and adding a region is a manual negotiation for space.
**Rejected:** a uniform grid (generic, no hierarchy) and a fully free-form layout
(no predictability; the header and strip would drift between revisions).

## D18 — Chrome marks hierarchy; it is not decoration

**Status:** Accepted
**Context:** Corner brackets were initially drawn on all six panels. A mark that
appears on everything marks nothing, and identical framing made the hero
indistinguishable from a battery readout.
**Decision:** The hero gets a 28 px chamfer, a second cut at the bottom-left, and
16 px corner brackets on its two square corners. Satellites get a 14 px chamfer
and no brackets. `theme_panel()` takes a `PanelChrome` argument so the
distinction is drawn by the theme, not decided by each panel.
**Consequence:** Hierarchy is visible in the frame itself, before any number is
read. Costs one enum and a branch in the frame routine.
**Rejected:** signalling the hero with colour. Colour is already spoken for —
yellow is primary and red is alert (D5).
**Updated by D40 and D78:** red is not the alert colour. The hazard band is the
only alert and it is yellow; red is the PLUGINS off-state token only.

## D19 — Accents reach meters and traces, never numbers

**Status:** Accepted (amends D5's "never in the data"); amended by D78 for the
PLUGINS status counts
**Context:** The palette assigns an accent per region, but an earlier rule said an
accent may appear only in the title, the frame outline, and the hero brackets.
Both cannot hold: if the accent never touched a panel's own content, the region
colour coding would be invisible, and STORAGE and CORE would differ only by a
word. The first render made the contradiction obvious — the hero trace is the
panel's whole point and it looked wrong in plain white.
**Decision:** An accent may colour that panel's meters and traces. It may never
colour a numeric readout; numbers are always `C_WHITE`, and emphasis in a
readout is size (`px=5`) rather than hue.
**Consequence:** Region identity is visible at a glance, and a coloured number
still means nothing in particular, so red keeps its monopoly on "wrong". Costs a
slightly longer rule.
**Amended by D78:** red no longer has a monopoly on "wrong"; it is the narrow
PLUGINS off token. The rule that a metric readout is never coloured still stands.

## D20 — `C_DIM` was raised to a measured contrast

**Status:** Accepted
**Context:** Labels and absent values were `#43444F` on `#0D0E12`. The host
preview made the failure visible: at 10 px the labels were barely present, and
the clock vanished entirely when it fell back to `--:--:--`.
**Decision:** `C_DIM` is `#6E707E`, about 4:1 against the panel fill instead of
about 2:1.
**Consequence:** Labels and `--` are legible at arm's length on a 5-inch screen
while staying clearly secondary to `#EDEDF0`. Costs some of the "almost
invisible" texture the darker value gave.

## D21 — A trace is a line with a declared axis

**Status:** Accepted
**Context:** The first implementation filled the area under each sample, as the
design sketch's block characters suggested. At 200 samples across ~500 px it
rendered as a solid rectangle that hid the shape it existed to show. Scaling the
hero trace from zero was worse: free memory sits near capacity, so the line
pinned itself to the top edge with 80 px of empty box beneath it.
**Decision:** Traces draw a 2 px polyline, joined vertically between samples so a
one-sample spike is a spike. The hero trace is scaled to the window's own
range, and that range is printed beside it. The FRAME trace keeps a fixed
0–33.3 ms axis, because there a spike means a dropped frame and that meaning
depends on the scale not moving.
**Consequence:** The trace shows shape, and because the axis is on screen the
autoscale is declared rather than implied. Costs a caption and a min/max pass
over 200 samples at 10 Hz.

## D22 — The host preview is a verification tool, not a build target

**Status:** Accepted
**Context:** No Vita is reachable from this environment, so nothing about the
layout could be checked. The port in D7 was justified partly by host testability,
and until this point that had not been used.
**Decision:** `tools/preview/` links the real `core`, `gfx`, `panels`, and `ui`
against a fake platform and a software stand-in for the four vita2d calls
`gfx/draw.c` makes, then writes PNGs.
**Consequence:** Layout, typography, palette, and the absent-value path can all be
inspected without hardware, and three real defects were caught this way. Costs a
second implementation of the four draw calls, which must be kept in step with
`gfx/draw.c`. It is never linked into `cockpit.self`, and it proves nothing about
the device: every reading it shows is synthetic.

## D23 — A failed power read is an alert condition

**Status:** Accepted (supersedes the condition list in D15)
**Context:** Code review found that `update_alert()` rebuilt the alert from zero
on every sample, and the only sensor-driven arm was gated on `power_valid`. The
moment `scePowerGetBatteryLifePercent()` started returning a negative value the
hazard band cleared, even though the battery was still low: a reader failure
silently cancelled an alert that was already true.
**Decision:** `!power_valid` raises the band on its own, alongside `!mem_valid`,
low battery while discharging, and `ux0:` not answering.
**Consequence:** The alert channel fails closed for the one reading that matters
most. Costs a fourth condition, and a console whose power API is broken shows a
band permanently — which is the correct signal for a broken power API.

## D24 — Screen geometry lives in `gfx/layout.h`

**Status:** Accepted (supersedes the location named in D11)
**Context:** D11 put `SCREEN_W`, `SCREEN_H`, `MARGIN` and `GAP` in `app/app.h`.
`panels/registry.c` and `ui/header.c` both need them and both sit inward of
`app/`, so neither could include them without inverting the dependency rule. The
result was that `MARGIN` and `GAP` had no readers at all: `registry.c` hardcoded
`18.0f` and `header.c` defined a private `HEADER_MARGIN`. Editing the constant
changed nothing and raised no error.
**Decision:** The four constants live in `gfx/layout.h`, which both consumers may
include. `registry.c` expresses its rects through `MARGIN` and `GAP`.
**Consequence:** A constant its consumers cannot reach is a constant that gets
hardcoded instead. Costs one header, and a second place to look for geometry.

## D25 — Catch-up is bounded; a stall's backlog is dropped

**Status:** Accepted (reverses "catch up rather than drop" from the original loop)
**Context:** The loop repaid every 100 ms of accumulated slow-sample debt inside
a single frame, with no cap. A long stall — suspend/resume is the everyday case —
became thousands of blocking devctl/power/RTC calls in one frame, and each
iteration pushed into the 200-sample ring, so the stall overwrote the whole
20-second trace with duplicates of the current value.
**Decision:** `dt` is clamped to `MAX_FRAME_DT` (0.5 s), at most
`MAX_CATCH_UP_SAMPLES` (3) slow samples run per frame, and any remaining debt is
dropped rather than replayed.
**Consequence:** A stall costs at most three samples and a visible gap in the
trace, which is honest. Costs the guarantee that every 100 ms of wall time is
represented in the history.

## D26 — The preview renders the shipped composition and drives the real sampler

**Status:** Accepted (supersedes the preview's independence in D22)
**Context:** The preview kept its own copy of `app.c`'s draw loop and set
`g_metrics` fields directly. Both were load-bearing failures: changing the draw
order left the preview rendering the old composition and still passing, and the
alert policy was never executed by anything — deleting all three arms of
`update_alert()` left the PNGs byte-identical.
**Decision:** `screen_draw()` moves to `app/screen.c`, which makes no SDK call, so
both `app.c` and the preview call it. Scenario control moves into a shared
mutable fake (`tools/fake/`), and the preview produces each scene by driving the
fake port through `metrics_sample_slow()`.
**Consequence:** A preview PNG now proves the policy ran. Costs the preview its
independence from the app's composition, which is the point.

## D27 — Readout scale is a parameter; the trace axis is computed at 10 Hz

**Status:** Accepted
**Context:** Three related defects. `theme_readout` fixed the px=2/px=3 pair, so
`storage.c` and `system.c` re-derived the baseline rule by hand; the SYSTEM
firmware row hand-rolled a px=5 value that overprints its own caption at ten
characters; and `draw_trace()` rescanned 200 samples twice per frame to build an
axis that only changes at the sample rate.
**Decision:** `theme_readout_px` takes both scales and `theme_readout` wraps it at
px=2/px=3. SYSTEM's firmware value is px=3, like every other readout in that
panel. The trace axis is computed once per slow sample and cached in `Metrics`.
**Consequence:** One baseline rule, one place to change a scale, and a draw loop
that does no history scanning. Costs two fields in `Metrics` and a SYSTEM panel
without a primary number.

## D28 — Static text is baked; glyphs and traces are batched

**Status:** Accepted (implements the deferred performance finding)
**Context:** One frame issued ~2,818 draw calls, of which 462 were the wordmark
alone: seven passes over a seven-letter constant string, each glyph run its own
`vita2d_draw_rectangle`. Two traces added 400 more. On a Cortex-A9 each call is
seven GXM entry points plus a pool allocation plus one `sceGxmDraw`.
**Decision:** `draw_rects()` batches same-coloured rects into one
`vita2d_draw_array`. `font_rects()` enumerates a string's glyph rects without
drawing, so the wordmark's 66 rects are baked once and replayed as four batched
passes (halo, chroma left, chroma right, solid). Every glyph draws its runs as
one batch, and each trace is one batch. `tools/preview` counts draw calls and
fails over a 600-call budget.
**Consequence:** A full frame with the hazard band up went from ~2,818 calls to
381, with identical pixels. Costs a baked rect list in `header.c` that must be
rebuilt if the wordmark's position or scale changes — it is keyed on
`s_wordmark_count <= 0`, so moving the wordmark means resetting that.
**Rejected:** baking the wordmark into a render-target texture. It would cut the
same calls but needs a GXM render target and hardware to confirm the result, and
the batching gets most of the win with no device-specific risk.

## D29 — Vertex data comes from the vita2d pool, never from a static buffer

**Status:** Accepted (fixes the first hardware crash)
**Context:** The first build installed and launched, then faulted the GPU on the
first frame. Disassembling `libvita2d.a` showed why: `vita2d_draw_array` calls
`sceGxmSetVertexStream(ctx, 0, vertices)` with the caller's pointer and performs
no copy. GXM reads that stream when the scene is submitted, so the buffer must
live in GPU-mapped memory until the frame ends. `draw_tri` passed a stack array
and `draw_rects` a BSS array; neither is mapped. `vita2d_draw_rectangle` and
`vita2d_draw_line` are safe because they allocate from `vita2d_pool_memalign`
internally — which is the pattern the two direct callers should have followed.
The two pre-fix GPUCRASH dumps in `docs/gpu-crash-study/` confirm the stack half of
this: both fault on the last 4 KiB page of the main thread stack
(`stack+0x3f000`), with the PDS vertex-data/uniform-fetch requestor
(`docs/gpu-crash-study/ANALYSIS.md`).
**Decision:** `draw_tri` and `draw_rects` allocate their vertices from
`vita2d_pool_memalign`. The pool is sized explicitly at 2 MiB in `main()`, since
vita2d does not check the allocation and a null block would fault.
**Consequence:** Both draw paths are valid on device. Costs one pool allocation
per batch, which is the same thing vita2d does per primitive.

**Why the host preview did not catch this.** The software shim reads vertices
immediately and renders into a plain array, so it never models GXM's deferred
read or its address-space rules. A stack pointer and a pool pointer look
identical to it. The preview now models *pool exhaustion* — it uses a bump
allocator the size of the device pool and fails the run if an allocation misses —
but address-space validity remains outside what any host shim can prove. That
gap is the reason a hardware session is still the only verification for this
class of defect.

## D30 — The render pool is sized explicitly, not defaulted

**Status:** Accepted
**Context:** Every vertex a frame submits is allocated from the vita2d pool and
must stay valid until scene submit. The host preview measures the worst-case
frame at 260 KiB (the alert scene, with both traces full and the hazard band up).
**Decision:** `main()` calls `vita2d_init_advanced(RENDER_POOL_BYTES)` with 2 MiB
rather than relying on `vita2d_init()`'s default, which the headers do not state.
**Consequence:** The pool is ~8x the measured worst case, leaving room for more
panels. Costs 2 MiB of GPU-mapped memory, which is trivial against the app
budget.

## D31 — The renderer uses only `vita2d_draw_rectangle` and `vita2d_draw_line`

**Status:** Accepted (supersedes the batching in D28; keeps D29's rule)
**Context:** The first hardware build faulted the GPU because `vita2d_draw_array`
was given non-mapped vertex memory (D29). After that was fixed with pool
allocations, the app launched but drew no text, no trace and no hazard stripes —
everything routed through `vita2d_draw_array` — while the panel chrome, drawn
with `vita2d_draw_rectangle` and `vita2d_draw_line`, rendered correctly. The call
graph, arguments, pool size (2 MiB, confirmed allocated) and primitive enum were
all verified correct in the shipped binary, so the defect is inside the
`vita2d_draw_array` path rather than in this code, and it cannot be reproduced in
the host shim.
**Decision:** `draw_rects` is a loop of `vita2d_draw_rectangle`. `draw_tri` is a
scanline fill that emits one horizontal rect per row. Neither `vita2d_draw_array`
nor `sceGxmSetVertexStream` is called anywhere in the renderer.

The GPU crash dumps in `docs/gpu-crash-study/` confirm the first half of this: both
COCKPIT faults are `GPU_BIF_ACCESS_FAULT` on the last 4 KiB page of the main
thread stack, which is the GPU fetching `draw_tri`'s stack vertex array. The
second half — no output from pool-backed `draw_rects` — is unexplained by the
dumps, because that build did not fault. Removing the primitive removes both.
**Consequence:** The renderer is back to ~2,900 draw calls per frame (3,641 with
the hazard band up) from 381, and every primitive it uses is one the device has
already proven. The chamfer cuts and hazard stripes are now stair-stepped
diagonals rather than true triangles, which is indistinguishable at this scale.
Costs the performance work in D28, which stays recorded: it can return once a
hardware session proves `vita2d_draw_array` works.

**Lesson for the host harness:** the preview proved geometry and typography, and
caught real defects, but it cannot model GXM's address-space rules or its
deferred vertex reads. Two consecutive hardware failures came from exactly that
gap. A preview that renders correctly is not evidence that the draw path is
valid on the device.

## D32 — Artifacts carry the build number

**Status:** Accepted
**Context:** Every rebuild produced `cockpit.vpk`, so successive builds overwrote
each other and there was no way to tell which one was on the console.
**Decision:** `build.sh` increments `BUILD_NUMBER` in the project root and passes
it as `COCKPIT_BUILD`; CMake names the package
`cockpit-v<version>-b<build>.vpk`. `send.sh` uploads the newest one.
**Consequence:** Builds coexist in `ux0:data` and the filename identifies them.
The counter lives outside `build/` so wiping the build directory does not restart
numbering into a collision. Costs one tracked counter file and a rebuild when it
needs resetting. `APP_VER` stays `##.##`; the build number is an artifact name,
not package metadata.

## D33 — STORAGE shows `ux0:` and `uma0:` only

**Status:** Accepted
**Context:** The panel carried three mounts. On the PCH-1000 this targets there
is no `imc0:` at all, so a third of the region said `--` permanently, and the
SD2Vita card that is actually interesting shares the panel with a row that can
never have data.
**Decision:** `PLATFORM_STORAGE_COUNT` is 2: `ux0:` then `uma0:`.
**Consequence:** Both rows can hold a real reading. Costs the internal memory
card on a PCH-2000 — if COCKPIT ever targets one, it comes back as a third slot
with a compile-time or runtime check, not as a permanent empty row.

## D34 — A failed storage read reports its return code

**Status:** Accepted
**Context:** The first hardware build showed `--` for every mount. A denied
`sceIoDevctl` (this is a safe eboot), a reply this code misreads, and a genuinely
absent card all rendered identically, so the screenshot could not distinguish
them and neither could the code.
**Decision:** `PlatformStorage` carries the platform call's return code, and the
STORAGE panel prints `E<rc>` when the call failed. The plausibility bounds are
temporarily reduced to `rc >= 0 && max_size > 0` so a valid reply cannot be
silently rejected for being in an unexpected unit; `free` is clamped to `total`
so the subtraction cannot underflow.
**Consequence:** The next hardware build is diagnostic: `E-<n>` means the call
itself is refused, a number means it succeeded and the value shows the unit.
Costs one temporary UI state and weaker validation, both to be removed once a
real reading is confirmed. This is the one place the "never show a number you
cannot vouch for" rule is deliberately relaxed, and it is time-boxed to that
confirmation.

**Result (build b10):** `ux0:` returned `0x80010030` and `uma0:` returned
`0x80010013`. Decoding the low byte against `sys/errno.h`: `0x13` is `ENODEV`
(19), which is the correct answer for an absent second volume. `ux0:` is a
different failure — the card exists, so the call is being refused rather than
finding nothing. That points at the safe eboot's permissions, which is what D36
tests.

**Result (build b14-unsafe):** `ux0:` returned a real size. Confirmed on
hardware: **`sceIoDevctl` on `ux0:` is refused under the safe authid and
permitted under `UNSAFE`.** D38 is the response — an ordinary app-manager call
reports the same numbers without the permission cost.

## D35 — The sweep is the liveness cadence, at 5 seconds

**Status:** Accepted
**Context:** A hardware report described the screen as "fixed". Sampling was not
at fault: `metrics_sample_slow` runs at 10 Hz (verified in the shipped binary,
three call sites in the loop). The readings themselves are simply stable — arm,
gpu and bus clocks do not move, nor do temperature, cycle count, state of health,
firmware or model. Only the wall clock, the uptime and the two traces change, and
they change slowly enough to read as a still image.
**Decision:** The interference sweep runs every 5 s instead of 7 s, so the screen
visibly refreshes at roughly the cadence a reader expects.
**Consequence:** The one device DESIGN already designates as "proves the render
loop is running" now does that job at a perceptible rate. Costs nothing; the
sweep is two rects.
**Rejected:** animating the values themselves. The design forbids animating a
value that did not change, and inventing motion would make a static reading look
like a live one — the exact failure the project is built to avoid.

## D36 — UNSAFE is available as a marked, test-only build

**Status:** Accepted (does not supersede D9; the shipped default is still safe)
**Context:** `ux0:` returns `0x80010030` from `sceIoDevctl` while the card is
plainly mounted. The remaining explanations are a privilege refusal under the
safe eboot's authid and a malformed call, and no amount of host testing
distinguishes them.
**Decision:** `CMakeLists.txt` gains `COCKPIT_UNSAFE` (default `OFF`) and
`build.sh` gains `--unsafe`. An unsafe build is named
`cockpit-v<version>-b<build>-unsafe.vpk` so it can never be mistaken for a
shipped artifact.
**Consequence:** One flash answers the question. If storage works under
`UNSAFE`, the permission is the cause and the product decision — ship unsafe, or
drop the STORAGE panel — becomes an explicit one. If it fails identically, the
call or the struct is wrong and the authid is irrelevant.
**Constraint:** `AGENTS.md` rule 3 still holds. This is the "specific API requires
it" case being *tested*, not assumed; nothing ships unsafe until the test says so
and a decision records it.

## D37 — Storage shows its own unit; an absent volume is not an error

**Status:** Accepted
**Context:** Two problems with the diagnostic from D34. `uma0:` reported
`ENODEV`, which is the correct answer for a volume that is not mounted — but it
printed as an error code, making a normal console look broken. And the value was
fixed to tenths of a GB, so a sub-gigabyte volume would render as "0.4/0.5 GB",
losing the precision that matters at that size.
**Decision:** `StorageSample` holds whole megabytes. The panel picks the unit
from the volume's own size — GB with one decimal at 1 GB and above, whole MB
below — and prints it in the value. `ENODEV` is classified as "absent" and
renders `--`; every other negative code still prints, because those are real
refusals.
**Consequence:** A card reads "12.4/29.5 GB", a small volume reads
"384/512 MB", and a missing one reads `--`. Costs a size-dependent format and a
`[C]` assumption that SCE errors carry the errno in the low byte — which is what
`ENODEV` matching exactly on hardware indicates. (The absence marker became
`N/A` in D41.)

## D38 — Storage uses `sceAppMgrGetDevInfo`, so the app stays safe

**Status:** Accepted (supersedes the devctl-only path; D9 stands)
**Context:** D36's unsafe build proved that `sceIoDevctl` on `ux0:` is refused
under the safe authid (`0x80010030`) and works under `UNSAFE`. That left a bad
choice: relax the whole system's permissions for one panel, or ship a dashboard
whose storage region is permanently blank.
**Decision:** `platform_read_storage` calls
`sceAppMgrGetDevInfo(dev, &max_size, &free_size)` first. It is an ordinary
app-manager call, reports capacity and free space in bytes, and needs no
permission relaxation. The devctl remains as a fallback for a device AppMgr does
not cover.
**Consequence:** Storage works under the shipped safe eboot and D9 is intact. The
unsafe path stays available for testing but is no longer required by anything
COCKPIT does.
**Rejected:** shipping `UNSAFE`. It would have worked, but it costs every install
a system-wide permission relaxation and an "Enable Unsafe Homebrew" requirement,
to read one number that a safe API already provides.

**Result (build b25, hardware):** `ux0:` reports `17.7/119.3 GB` under the safe
authid. The path works, D9 stands, and the unsafe build is no longer needed by
anything COCKPIT does.

## D39 — No in-app exit button

**Status:** Accepted
**Context:** START was bound to exit. A dashboard is meant to be left running, so
a stray press dismissed the screen for no reason — reported on hardware as the
app "minimising".
**Decision:** The binding is gone. `app_run` loops until the system closes it;
the PS button is the way out, as it is for any Vita app.
**Consequence:** The app can no longer be dismissed by accident, and the
controller dependency disappears entirely — `psp2/ctrl.h`, `SceCtrl_stub` and
`sceCtrlSetSamplingMode` are all removed, leaving `app.c` with one SDK call (the
process clock). Costs an exit path that depends on the system, which is how the
platform expects apps to behave anyway.

## D40 — No red, and no bar after the wordmark

**Status:** Superseded by D78 (red returns as the narrowly scoped off-state
token)
**Context:** Two pieces of chrome were doing nothing useful. A 1 px red rule sat
on top of the hazard band; with the band already reading as a warning, the rule
added a colour the rest of the screen never used. And a short vertical bar
followed the wordmark purely as punctuation — it encoded nothing, which is
exactly what the design forbids.
**Decision:** Both are removed. `C_RED` is deleted from the palette; the hazard
band is yellow-and-black and is identified by its stripe pattern. The wordmark is
the word.
**Consequence:** The palette has one accent pair and no alert colour, so nothing
can be mistaken for a warning that is not the band — and nothing can be mistaken
for decoration that is a warning. Costs the ability to signal urgency with hue,
which the stripe pattern already does better at this size.
**Rejected:** keeping red as a reserved token for a future alert treatment.
Nothing used it, and an unused palette entry is a rule waiting to be broken by
whoever finds it first.

## D41 — STORAGE never renders an empty row

**Status:** Accepted
**Context:** With a second volume mounted, `uma0:` reported the same
`0x80010030` refusal that `ux0:` first hit: `sceAppMgrGetDevInfo` documents
itself for `ux0:` only, so it does not cover `uma0:`, and the devctl fallback is
refused under the safe authid. That surfaced as a ten-digit number on the HUD.
Separately, an unmounted slot rendered nothing at all, which reads as a broken
panel rather than an empty slot.
**Decision:** A storage row always states its condition. A mounted volume shows
its size; an unmounted one shows `N/A`; a refused read shows `ERR` plus the errno
byte (`ERR 30` is the permission refusal). No row is ever blank, and no raw SCE
code reaches the screen.
**Consequence:** Every state of the panel is legible at a glance, and the code
stays diagnostic without being unreadable. Costs the `--` convention for this
panel: `N/A` is deliberate, because "no volume in the slot" is a different
statement from "this metric could not be read", which is what `--` means
everywhere else.
**Open:** `uma0:` capacity needs a call the safe authid refuses. Under `UNSAFE`
the devctl path reports it. Whether that is worth the permission cost is a
product decision, not a code one.

**Resolved on hardware:** with `uma0:` mounted, the unsafe build reports its
capacity and the safe build reports `ERR 30`. The limitation is permissions, as
suspected — `sceAppMgrGetDevInfo` covers `ux0:` only, and `sceIoDevctl` is
refused. Which build ships is the product decision left open above.

## D42 — UNSAFE is the default eboot

**Status:** Accepted (supersedes D9)
**Context:** D9 chose a safe eboot because nothing in COCKPIT needed private
APIs. That held until storage. `uma0:` capacity is reachable only through
`sceIoDevctl`, which the safe authid refuses with `0x80010030`, and
`sceAppMgrGetDevInfo` covers `ux0:` only. Confirmed on hardware: the same binary
reads both volumes under `UNSAFE` and only `ux0:` under safe.
**Decision:** `UNSAFE` is the default. `build.sh --safe` produces the restricted
variant, marked `-safe` in the artifact name.
**Consequence:** Both storage rows read, which is what the panel exists to show.
Costs: the console must have "Enable Unsafe Homebrew" on, the install warns, and
the app runs with system-wide permissions it does not otherwise need. That cost
is real and accepted, because the alternative is a dashboard that cannot read
half its own storage.
**Rejected:** shipping safe with `uma0:` reading `ERR 30`. A system dashboard
that refuses to report a mounted volume is not doing its job, and the failure
would look like a bug in COCKPIT rather than a deliberate restriction.

## D43 — PLUGINS panel: a list strip, and where its space came from

**Status:** Accepted (amends the layout in D17)
**Context:** The dashboard had no way to answer "what is taiHEN actually
running", which is the first thing to check when a console misbehaves. Enso
loads plugins from `ur0:tai/config.txt` and the legacy `ux0:tai/config.txt`, and
skips any path whose file is missing — so the panel has to read the configs *and*
stat each entry, or it would report plugins that are not loaded.
**Decision:** A new PLUGINS region, 552 x 68, in the left column directly above
FRAME. It uses `CHROME_LIST`: the satellite chamfer, the title top-left and
inline, no rule. Space came from CORE (240 -> 190, the trace absorbing the loss)
and STORAGE (144 -> 114); the right column is untouched.
**Consequence:** The dashboard can name what is running without disturbing POWER
or SYSTEM, which are already tight. Costs the hero trace 36 px — it is still the
panel's escalated element, just shorter — and costs STORAGE some breathing room.
**Rejected:** a full-width strip. It read better for name-plus-description, but
clearing space above it meant squeezing POWER to 161 px, where its four detail
rows collided. A narrower panel at the micro type scale keeps every other region
readable, which matters more than the list's comfort.

## D44 — File access crosses the port; config parsing does not

**Status:** Accepted
**Context:** Reading `config.txt` is a device detail, but deciding which lines are
plugins, resolving descriptions and dropping entries whose file is missing is
policy — and that is the part most likely to be wrong.
**Decision:** `core/platform.h` gains `platform_read_text_file` and
`platform_file_exists`. `core/plugins.c` parses the text it is given and never
touches a file, so the parser runs in the host tests against sample configs.
**Consequence:** The config format is exercised off-device, including the awkward
cases — CRLF endings, indented paths, comments, section headers, a plugin listed
in both configs, and an unknown plugin that must keep an empty description rather
than an invented one. Costs two more port functions.
**Note:** descriptions are `[C]` community knowledge about common plugins, held in
one table. An unknown plugin still appears with no description, which is honest;
a guessed description would not be.

## D45 — The panel area scrolls; the header and status strip are pinned

**Status:** Accepted (supersedes the fixed single-screen assumption in D13/D17);
the input half is superseded by D76
**Context:** Every new region had to be paid for out of another one. By the time
PLUGINS arrived, CORE's trace had been cut to 29 px and POWER's four detail rows
were colliding, purely to keep everything on one 960x544 screen. A dashboard that
grows will never fit a fixed screen, and squeezing to fit makes the regions that
are already there worse.
**Decision:** The layout table is laid out on a canvas taller than the screen
(`g_canvas_h`, 620 when written; now computed from the plugin count, D47).
`screen_draw` offsets every panel rect by the
scroll position and clips to a 396 px viewport. The header and the FRAME strip are
pinned: the clock and frame health stay visible at every scroll position. The
d-pad and the left stick scroll, at 900 px/s at full deflection, clamped to the
canvas. A thin scrollbar on the right edge shows position, because a scrollable
screen with no affordance looks like a screen that simply ends.
**Consequence:** Regions get their natural size again — CORE back to 240, POWER to
216 (both 210 since D48) — and the next region is a table edit rather than a
negotiation. Costs a
viewport clip (a GPU state change per frame), a scroll position the app owns, and
the controller dependency that D39 removed, now justified by scrolling rather than
by an exit button.
**Rejected:** scrolling the whole screen including the header. It would be simpler,
but the clock and frame time are the two readings that should never scroll away.

## D46 — A build number counts builds, not invocations; only one artifact is kept

**Status:** Accepted (amends the artifact rule in D32)
**Context:** The counter reached 46 for roughly twenty real builds, and the build
directory held 44 near-identical VPKs. Two causes. Rebuilding unchanged sources —
which happens constantly while checking for warnings — incremented the number, so
the counter tracked keystrokes rather than builds. And nothing ever deleted a
superseded artifact, so every number left a file behind.
**Decision:** `build.sh` hashes the source tree and reuses the previous number
when the hash is unchanged; a rebuild with no edits produces the same artifact
name instead of a new one. After every build it deletes all but the newest VPK.
**Consequence:** The number matches the number of distinct builds, and the build
directory holds exactly one artifact — the one you would flash. Costs a `shasum`
over the sources per build (milliseconds) and loses local history: an older build
can only be rebuilt, not recovered from disk.
**Rejected:** keeping the last N artifacts. "Which one do I flash" then becomes a
question, and the answer is always the newest.

## D47 — The plugin list is a self-sizing two-line ledger

**Status:** Accepted
**Context:** The first PLUGINS panel was a fixed-height strip showing name and
description side by side, clipped to three rows with a "+N more" count. That
hid plugins, and the region's height was a guess that had to be renegotiated
every time the screen changed.
**Decision:** One row per plugin, two lines each: the name, a leader rule to a
status dot on the right, then the description underneath. The panel computes its
own height from the row count (`plugins_panel_height()`), and `layout_update()`
writes that height into the layout table and recomputes the canvas height, so the
region grows or shrinks with the list and nothing is clipped.
**Consequence:** Every loaded plugin is visible, the canvas is exactly as tall as
its content, and adding a plugin needs no layout edit at all. Costs a mutable
layout table (`g_layout` and `g_canvas_h` are no longer `const`) and one
`layout_update()` call after the plugin scan.
**Descriptions:** the full AutoPlugin2 Vita catalog
(`AUTOPLUG2/plugins/plugins.lua`) and its English strings
(`AUTOPLUG2/lang/ENGLISH_US.lua`), keyed by plugin file name and shortened to one
line. 155 rows. Rows marked `[C]` are not straight catalog copies: libshacccg and
iTLS are community knowledge, YAMT's text is from its README (the `.suprx` is the
settings GUI), HENkaku's is from its plugin source, and ioPlus/kuio are catalog
dependencies with no standalone entry. Four catalog rows carried another
plugin's text or unusable text (adrenaline_kernel, ds3, lowmemmode, scorehax) and
were rewritten. An unknown plugin still appears with no description rather than a
guessed one.
**The green dot** is `C_GREEN`, the only status hue in the palette. It marks a
row as loaded; every row here is loaded, because the scan drops entries whose
file is missing. It is not an accent and must not be used anywhere else.
**Superseded by D73 and D78:** the dot was removed with the row redesign, and
`C_GREEN` is now also the rocker's on half and the ACTIVE count. The
ledger/self-sizing decision above still stands.

## D48 — The vertical rhythm is a 4 px scale, and the grid has one seam

**Status:** Accepted
**Context:** The first screen passed the design review but read as unpolished on
hardware. Measured, the gaps between consecutive text blocks were 2, 3, 4, 5, 5,
6, 7 and 13 px against type 21-35 px tall. A 35 px battery figure sat 2 px above
its state caption; a 21 px value sat 5 px above the next row. Separately, CORE
was 240 tall while POWER was 216, and STORAGE started 24 px below SYSTEM, so the
horizontal seam between the two rows stepped across the screen.
**Decision:** One spacing scale, `SP_1`-`SP_6` (4/8/12/16/24/32), declared in
`theme.h`. Panel content starts at `PANEL_BODY_Y` (the title rule plus `SP_4`);
readout rows use `READOUT_STRIDE` (`LINE_H(PX_VALUE) + SP_4` = 37); a block is
separated from the next by at least `SP_4`, and the space inside a block is at
most `SP_2`. Both columns share both seams: row one is 210 tall, row two 172.
**Consequence:** Every gap is on the scale and derived from the type ladder, so
changing a font size cannot silently close a gap. The two columns read as one
grid. CORE's trace grew from 29 to 40 px because the panel it sits in shrank by
30 px, and the 93 px of dead space under it is gone.
**STORAGE:** matching SYSTEM's 172 px exposed a problem: two inline rows left
55 px of empty panel. The meter moved below its readout and spans the content
width, so each volume is a two-line block and the rows fill the panel. This is a
local change to STORAGE, not a new pattern; the readout and the meter are the
same widgets.
**Also fixed:** the absent meter track was `C_HAIR` at 45 %, which rendered at
(12,13,16) against a (13,14,18) fill -- invisible. It is now 70 % (19,20,25).
The FRAME strip's FPS/MS units were 3.5 px above the value baseline; they now
share it.
**Rejected:** keeping the old gaps and only increasing the panel heights. The
panels were not the problem; the rhythm inside them was.

## D49 — Derived layout state, and draw only what the viewport can show

**Status:** Accepted (supersedes the `g_canvas_h` sentence of D47)
**Context:** A whole-project simplification review found two kinds of drift. The
canvas height was a mutable global written twice from the same formula, and the
scroll limit was recomputed in three files; a reordered or resized panel could
leave either stale. Separately, every panel was drawn at every scroll position,
including panels entirely outside the 396 px viewport: at scroll 0 the plugin
panel starts 10 px below the band and was still emitting its chrome, title and
every row.
**Decision:** `layout_canvas_height()` and `layout_max_scroll()` derive both
values from the layout table; `g_canvas_h` is gone. `layout_update()` resolves
the PLUGINS slot by panel identity rather than by index 4, and only writes that
rect. `screen_draw()` skips a panel whose rect does not intersect the view band,
and the plugin ledger skips rows outside it. On the same pass, the storage errno
byte is decoded in `platform_psp2.c` and carried across the port, so that `[C]`
claim no longer reaches a panel; the four panel insets share `PANEL_INSET`; and
the readout formatters share `THEME_ABSENT` and `theme_readout_int()`.
**Consequence:** A missing `layout_update()` fails closed to the compiled-in
heights, and a reordered table cannot resize the wrong panel. The preview's
draw-call count fell from 4,153 to 2,867 at scroll 0 and from 4,875 to 3,578 at
the bottom. Culling is pixel-exact: the band is already clipped, and every panel
draws inside its own rect.
**Rejected:** caching panel geometry or the trace columns. Culling alone paid for
the frame, and a cache adds state that a scroll offset makes easy to get wrong.

## D50 — Keep the whole plugin list, and make a ceiling visible

**Status:** Accepted (extends D47)
**Context:** `PLUGINS_MAX` was 12, chosen while the panel was still a readout.
A real 3.65 config listed 19 plugins: `add_plugin()` stored the first 12 and
dropped the rest without a trace, so the panel presented a short list as if it
were complete. The name field was 32 bytes while the catalog already contained a
41-character file name, which cut the displayed name and made its description
lookup miss. A path longer than its buffer was truncated, then failed the
existence check, dropping the row with no explanation.
**Decision:** The array holds 64 plugins (`PLUGINS_MAX`), the name field 48
bytes (`PLUGIN_NAME_MAX`), and the path 96 (`PLUGIN_PATH_MAX`). Duplicate
detection runs before the ceiling, so a repeated line such as `henkaku.suprx`
under several sections is not counted as a lost plugin. A path too long for the
buffer is skipped rather than truncated. If the ceiling is reached,
`PluginList.truncated` is set and the panel prints `MORE PLUGINS NOT SHOWN` in
the slot a further row would use; `plugins_panel_height()` accounts for the
line, so it cannot be clipped.
**Consequence:** A 19-plugin config renders all 19 rows, and the canvas grows to
match because D49 derives it from the layout table. A 19-row scratch render
measured 4,740 draw calls against the 5,500 budget. Comments are deactivated
entries and never reach the list. Host tests are 74.
**Rejected:** a dynamically allocated list. `plugins_scan()` runs once at
startup, so a heap list would work, but it adds an allocation-failure path and a
lifetime to a module that currently has neither. 64 is more than a console can
usefully boot; the flag covers the impossible case instead of hiding it.

## D51 — Group the plugin ledger by load target

**Status:** Accepted (extends D47 and D50)
**Context:** The ledger was a flat list, but `config.txt` is organised by load
target: `*KERNEL`, `*main`, `*NPXSxxxxx` system apps, `*ALL`, and individual
title IDs. A flat nineteen-row list hid that structure and made a user compare
the panel against the file line by line to see what ran where. The user asked
for the config's grouping with friendly names.
**Decision:** The parser records a `PluginSection` for every entry and the panel
draws a tracked cyan header once per non-empty group: KERNEL, SYSTEM (`*main`),
SYSTEM APPS (`*NPXS*`), ALL TITLES (`*ALL`), TITLES (any other target). A file
listed in several sections is shown once, under the most privileged one, which
is the enum order: KERNEL, SYSTEM, SYSTEM APPS, ALL TITLES, TITLES. A path
before the first section header is ignored; the format gives it no load target
(`[C]`, no such config has been observed).
**Consequence:** The 19-entry console config renders four groups: KERNEL 9,
SYSTEM 8, SYSTEM APPS 1, ALL TITLES 1. `henkaku.suprx`, listed under a system
app and under `*main`, appears once in SYSTEM. Panel height becomes
`41 + 26 × groups + 52 × rows + 10 × gaps`, so the canvas is 1,581 px for that
config and scrolls. A 19-row scratch render measured 4,423 draw calls against
the 5,500 budget.
**Rejected:** one row per section occurrence. `henkaku.suprx` would appear three
times and the panel would answer "how many lines are in config.txt" instead of
"what is running".

## D52 — A plugin with no catalog entry says N/A

**Status:** Accepted
**Context:** A row outside the 155-entry catalog kept an empty description line.
That reads as a drawing failure or a clipped row, not as "the catalog has
nothing for this file", and the user asked for the absence to be explicit.
**Decision:** `draw_row()` always draws the second line. An empty description
renders `N/A` at micro scale in the same dim tone as a real description. The
data layer still stores the empty string: only the panel supplies the token.
**Consequence:** Every plugin row is exactly two lines, so the 52 px stride is
uniform and the panel height formula needs no special case. The preview's fake
config includes one unknown file (`mystery.suprx`) so the N/A path is visible in
every render. Host tests keep asserting that an unknown entry's description is
empty, which pins the boundary between data and presentation.
**Rejected:** writing `N/A` into `PluginInfo.desc` during the scan. It would
make an unknown plugin indistinguishable from one whose catalog text is
literally "N/A", and it puts UI copy in the sampling layer.

## D53 — Inset the scrolling band, and drop the header hairline

**Status:** Accepted
**Context:** CORE and POWER started at the viewport top (y 74), exactly where
the 2 px panel outline is drawn. The scissor cut the upper half of that outline,
so the two panels looked like they continued under the header. A 1 px `C_HAIR`
rule at y 47 also ran under the clock and read as an unearned divider; the
header's only rule should be the hazard strip, and only on a fault.
**Decision:** The scroll band starts at y 64, eight pixels below the hazard
strip, and ends at 472. Content is inset by `VIEW_TOP_GAP` (12), so the first
panel begins at screen y 76 with a whole outline. `layout_max_scroll()` adds the
inset, which keeps the scroll range identical to the old 396 px band. The
scrollbar track starts at the content top (76), not the band top. The hairline
is deleted; `header_draw()` paints the wordmark, the clock, and the hazard strip
only.
**Consequence:** At scroll 0 the two-row canvas sits at 76..470, with 12 px of
air before the FRAME strip at 482. While scrolling, content clips at 64, eight
pixels below the hazard, and the inset scrolls away. `C_HAIR` remains the
scrollbar track and the empty meter cell, nothing else.
**Rejected:** raising `VIEW_Y` while keeping the band 396 px tall. The outline
would be clipped at the new edge, and shrinking `VIEW_H` to compensate would cut
the bottom of STORAGE and SYSTEM at scroll 0. Moving FRAME down would buy the
room from the bottom margin the layout already spends.

## D54 — Tabbed screens with one explicit UI state

**Status:** Accepted (supersedes D16's single-screen assumption)
**Context:** COCKPIT was one screen, and D16's metrics global was written by one
sampler and read by the panels. A second screen adds a second consumer set but
does not add a second writer.
**Decision:** `UiState` in `ui/ui_state.h` owns the active tab, both scroll
offsets, the selected row, and the modal. `screen_draw(t, ui)` receives a
read-only pointer. HOME and PLUGINS share the header and FRAME strip; only the
region between them changes. The metrics global keeps its single writer.
**Consequence:** Tab state is not hidden in a panel or a second global. The old
`layout_update()` and the PLUGINS slot in the home layout table were removed and
HOME had a fixed canvas and no scrollbar. **Amended by D67:** the read-only
summary block returns a slot below the columns, so HOME scrolls again; the state
still lives in one `UiState`.
**Rejected:** a global `scroll` per panel and an `if (tab)` branch inside every
panel. Both make a third tab a rewrite instead of an addition.

## D55 — The active plugin config follows taiHEN, not the old app order

**Status:** Accepted (supersedes the ur0-first scan; the invalid-file half is
superseded by D65)
**Context:** taiHEN opens `ux0:tai/config.txt` first and falls back to
`ur0:tai/config.txt`. COCKPIT read ur0 first, so on a console with both files
the dashboard showed the shadowed copy.
**Decision:** Resolve ux0 first; use ur0 only when ux0 cannot be read. A
readable empty ux0 file is active. A too-large or invalid file is read-only.
The active path is frozen at launch, and PLUGINS names the active path and any
ignored one.
**Consequence:** The dashboard now describes the file taiHEN actually loads. A
stale ux0 no longer hides a working ur0 silently.
**Updated by D65:** a path before the first section is a warning, not a
read-only state, and a BOM does not hide the first section.
**Rejected:** merging both files, and editing ur0 while ux0 shadows it.

## D56 — A byte-preserving line model, keyed by full path

**Status:** Accepted
**Context:** The old parser dropped comments and missing files, merged entries
by basename, and kept no offsets, so it could not support a toggle.
**Decision:** The editor keeps every line's raw span, line ending, section,
active flag, and reachability. Rows are keyed by a normalized, case-insensitive
full path. Section names are case-sensitive and `*!Section` halts parsing. A
row shows its basename unless two rows share it, in which case the full path is
shown.
**Consequence:** A toggle changes only the `#` markers on the lines it names.
Two files with the same basename in ur0 and ux0 stay distinct.
**Rejected:** re-emitting a normalized config, and basename identity.

## D57 — The PLUGINS screen is an editable ledger with transactional writes

**Status:** Superseded by D61 and D64 (the `.bak` rotation, the `.bak`
precondition, and rename-over are gone; the row is still an editable ledger);
the toggle timing is superseded by D72 (stage first, write once)
**Context:** The ledger was read-only; a config write can leave a partial file.
**Decision:** PLUGINS is a full-screen ledger. Cross toggles a row. A write
removes a stale `.new`, writes a sibling, verifies it by read-back, checks the
active `.bak`, and renames the sibling over the original. At session start the
editor rotates `.bak` to `.1.bak` and `.1.bak` to `.2.bak`, then copies the
current file to `.bak`. A failed active-config backup makes the manager
read-only; a failure on another file only warns. `boot_config.txt` is read,
never written.
**Consequence:** A write never truncates the live file. The previous three
sessions are recoverable from the tai directory, and `.new`/`.bak` files are
excluded from the orphan list.
**Rejected:** in-place truncate-and-write; a single overwritten `.bak`; editing
`boot_config.txt`.

## D58 — Lock the recovery surface, warn the load-bearing plugins

**Status:** Accepted
**Context:** Research found nothing in `config.txt` is boot-critical, but
`henkaku.suprx` is the HENkaku Settings and reload surface, and a storage
driver in `config.txt` can be load-bearing for `ux0:`.
**Decision:** Lock `henkaku.suprx` (it can be restored but not disabled). Lock
`taihen.skprx`, `henkaku.skprx`, and `yamt.skprx` by name if they appear in
`config.txt`. Warn on `0syscall6`, `nonpdrm`, `repatch`, `nopsmdrm`,
`libshacccg`, `yamt.suprx`, `yamt_helper`, `reVitaMotion`, `storagemgr`,
`usbmc`, and `usbmcm`. KERNEL enables always confirm; the warned files confirm
in both directions.
**Consequence:** The manager cannot remove its own recovery UI or silently
disable a mount driver. A KERNEL disable that is not warned stays low-friction.
**Rejected:** locking every warned plugin, and warning every KERNEL disable;
neither has evidence that the entry can stop a boot.

## D59 — HOME counts the launch activated set; pending is session-scoped

**Status:** Accepted
**Context:** The user wants a count of activated plugins, not a count of edits.
A pending enable is not loaded until the reboot.
**Decision:** HOME counts the launch config's activated set: enabled, reached,
and present on disk. A pending enable does not count until the reboot; a
pending disable keeps counting. Pending is a per-line diff against the launch
snapshot. The baseline is session-scoped and is not persisted.
**Consequence:** A restart before the reboot re-reads the edited file as the
new baseline, so the pending marker is a session convenience. This is
documented; persisting it would need a boot identity the platform does not
expose reliably.
**Rejected:** reflecting pending edits in the total; a persisted baseline with
a guessed boot marker.

## D60 — Tabs and the HOME summary live in the header chrome

**Status:** Superseded by D63 (the summary no longer shares the hazard line)
**Context:** The old ledger slot starts at screen y 482, below the 472 band, so
a summary there is invisible at launch.
**Decision:** Tabs sit between the wordmark and clock at y 20, with a 2 px
yellow underline on the active tab. The HOME summary shares the hazard line at
y 48, right-aligned in micro scale; a fault replaces it with the hazard band.
When the run would cross the tab block, drop the word ACTIVE and then group
counts from TITLES upward; the total and pending token always survive.
**Consequence:** HOME keeps both panel rows at scroll 0 and has no scrollbar;
the summary costs no viewport height.
**Rejected:** a new pinned band (reduces the viewport), and a summary in the
old ledger slot.

## D61 — Two config snapshots, and `boot_config.txt` is out of scope

**Status:** Accepted (supersedes D57's backup model)
**Context:** The three-deep rotation rewrote an existing `.bak` by
rename-over-existing, which the adapter marks `[C]`; a failed copy therefore
made the manager read-only before the user touched anything. The user asked for
one first-session original and one per-session snapshot. Separately, the app was
reading `boot_config.txt` and creating `boot_config.txt.bak` copies, which is
outside a config manager's remit.
**Decision:** The active config gets exactly two snapshots. `config.txt.original`
is written only when it does not exist and is never overwritten. `config.txt.session`
is written from the session-start bytes on every launch and is overwritten each
session. No `.bak`, `.1.bak`, or `.2.bak` is created. `boot_config.txt` is never
opened, read, copied, renamed, or written; the three well-known boot module names
are locked by name only. A failed snapshot sets `BACKUP WARNING` but never makes
the config read-only.
**Consequence:** A bad edit has two restore points, and a backup failure cannot
lock the editor. The old `.bak` files from b28 are ignored and may be deleted by
the user; COCKPIT does not touch them.
**Rejected:** keeping the rotation; deleting `boot_config.txt.bak` from the app;
backing up both mounts when only one is active.
**Amended by D70:** `config.txt.undo` is the in-app restore point; `.original`
and `.session` stay snapshots for inspection and PC recovery.
**Amended by D74:** `.original` and `.session` are now offered as in-app restore
sources through the read-only preview; `.undo` becomes the contextual last-write
undo rather than the only restore point.

## D62 — The toggle is a two-state `#` edit

**Status:** Accepted (supersedes D57's three-state cycle and `.bak` precondition);
amended by D72 (the edit is staged, not written, until the dock commits)
**Context:** The old action had a third `RESTORE` state and refused to write
unless `.bak` matched the launch bytes. Both made the switch unpredictable.
**Decision:** A row is either fully on or fully off. `ENABLE` removes the
comment run from every reached line that names the path; `DISABLE` inserts one
`#` before the path on every reached line. A line already in the target state is
unchanged. Every other byte, including indentation, trailing spaces, CRLF, and a
missing final newline, is preserved. The session snapshot is the restore path.
**Consequence:** The control always reads `ENABLE` or `DISABLE` for an editable
row. A partial launch row normalizes to all-on or all-off; pending reflects the
difference from the launch pattern until the file matches it again.
**Rejected:** a third restore action; re-emitting the whole file; rewriting a
line's whitespace.

## D63 — The HOME summary is permanent chrome

**Status:** Superseded by D67 (the summary is a HOME block, not header chrome)
**Context:** The summary was drawn at `SUMMARY_Y 48`, exactly where the hazard
band was drawn, and in micro `C_DIM`. A fault covered it, and on hardware it was
easy to miss.
**Decision:** HOME always draws `N PLUGINS ONLINE` (singular for one) at label
scale on its own header line, with a green dot and a micro detail run for the
non-empty groups and pending count. The hazard band moves to y 56, the bottom
8 px of the header, and never covers the summary. A config problem is named:
`NO CONFIG.TXT`, `CONFIG UNREADABLE`, `CONFIG TOO LARGE`, or `CONFIG WARNING`.
**Consequence:** The count survives an alert. The total and pending token
survive the fit loop; group counts drop first.
**Rejected:** a new pinned band; putting the summary back in the panel area;
letting a fault replace the count.

## D64 — Read first, then stat; replace by rename-away and verify

**Status:** Accepted (supersedes the write half of D57)
**Context:** The read required `sceIoGetstat` before `sceIoOpen`, so a stat-only
failure looked like a missing config. It accepted a short read as complete, and
the write depended on `sceIoRename` replacing an existing file.
**Decision:** `platform_read_config` opens first and reads to EOF or the ceiling.
The stat is used only to classify an open failure and to reject a directory or a
short read. `platform_write_config` writes and verifies `<path>.new`, moves the
original to `<path>.old`, moves the verified sibling into place, reads the live
file back, and restores `<path>.old` if any step fails. The normal path never
renames over an existing file.
**Consequence:** A short read cannot become a truncated rewrite. A write reports
success only after the live bytes were observed. If the final replace is refused,
the original is restored and the row is unchanged. `platform_write_error()`
reports `OPEN`, `DATA`, `CLOSE`, `VERIFY`, `MOVE`, `RENAME`, `READBACK`, or
`RESTORE`, so the error line names the failing stage. The recovery path still
uses a rename over the live file when verification fails; if that is refused,
both files are left for manual recovery.
**Rejected:** truncate-and-write; trusting the rename's return value; a
read-back only of the sibling.
**Amended by D71:** a failed final rename reports `RESTORE` when the original
could not be put back, and an absent live file with a surviving `.old` is
recovered at the next scan.

## D65 — A BOM and a stray path are data, not corruption

**Status:** Accepted (supersedes the invalid-file half of D55)
**Context:** A UTF-8 BOM before the first `*KERNEL` made the parser miss the
section; the next path was "before a section", which zeroed the HOME count and
made the editor read-only. A single stray path therefore hid a valid config.
**Decision:** Both parsers skip a BOM at offset 0 and preserve it in the file.
A path before the first section is a warning: the row is `NOT REACHED`, the
valid entries still count, and the config stays editable. Only an unreadable,
too-large, too-long, or too-many-rows file is read-only, with the reason named.
**Consequence:** A Windows-edited config or one stray line no longer disables the
manager. The owner sees `CONFIG WARNING` and can repair the file.
**Rejected:** treating the whole file as invalid; stripping the BOM on write.

## D66 — The row owns its badge and control

**Status:** Superseded by D73 (the labelled action control and the reason
dialog; the per-row badge and the "every row is selectable" rule survive)
**Context:** The page had one status string and no visible action. Selection
skipped non-actionable rows, so a missing or locked row could not be selected to
read why it would not act.
**Decision:** Every row draws a status badge (`ACTIVATED`, `DISABLED`, `MISSING`,
`PARTIAL`, `NOT REACHED`, `BOOT`), an optional `PENDING` badge, and an action
control (`ENABLE`, `DISABLE`, `LOCKED`, `MISSING`, `BOOT`, or `--`). The control
is greyed when `editor_row_actionable()` is false; Cross on a greyed row opens a
short reason instead of acting. Every row is selectable. A missing active row
may be disabled; a missing inactive row may not be enabled.
**Consequence:** The control and the policy come from the same module, so the
button cannot promise an action the editor will refuse. Scrolling past the last
row reaches the installed-but-unlisted section, and Cross is ignored when the
selected row is off-screen.
**Rejected:** a hidden Cross-only action; skipping locked rows in the cursor;
a switch drawn from panel-local policy.
**Amended by D69:** the status labels are `LOADED`/`OFF`/`MIXED`, and the
per-row `PENDING` badge is `NEXT ON`/`NEXT OFF`.
**Removed in D73:** `editor_row_actionable()`, the labelled action control, and
the reason dialog. The row now draws a split rocker; the badge and the header
exception carry the state, and Cross stages an actionable row without a dialog.

## D67 — The plugin summary is a HOME block, not header chrome

**Status:** Accepted (supersedes D63)
**Context:** The header line added in b28 was easy to miss and read as chrome
rather than as one of the dashboard's readings. The owner asked for the
activated-plugin summary to return to the slot the old ledger occupied, below
STORAGE and SYSTEM, as a block rather than a status line.
**Decision:** HOME draws a full-width PLUGINS satellite block at canvas y 406,
924 × 140, below both columns. It carries the activated count at primary scale,
`PLUGINS ONLINE` at label scale, and one micro line of non-empty group counts.
`N PENDING` and `CONFIG WARNING` share the right end of that line. The block
reports the launch set — what taiHEN actually loaded — and is read-only. The
HOME header summary is removed; the header carries only the wordmark, tabs,
clock and hazard band, and the band returns to y 48.
**Consequence:** The count is no longer visible at scroll 0; the block sits just
below the fold and HOME scrolls 150 px to reach it, with the scrollbar as the
affordance. The block never grows with the plugin count, so it needs no height
cap. The PLUGINS tab keeps its own header line naming the active config and the
editor state.
**Rejected:** keeping the count in the header; listing every activated plugin in
the block (that is the ledger's job); two half-width blocks under each column.
**Amended by D68:** the block no longer shows `N PENDING`; pending stays on the
PLUGINS tab that owns the edit.

## D68 — Pending is a PLUGINS-tab state, not a HOME reading

**Status:** Accepted (amends D67); amended by D69 and D77 (the PLUGINS pending
surface is `NEXT ON` / `NEXT OFF`; the header counts the working set)
**Context:** The HOME summary block showed `N PENDING` beside the group counts.
The block exists to answer "what is running now"; a pending edit is bookkeeping
for a change that has not happened, and mixing it into a running count made the
block read as two states at once.
**Decision:** HOME never renders a pending token. The block reports only the
launch set, its group counts, and `CONFIG WARNING`. Pending remains visible on
the PLUGINS tab, where the edit is made: the header line, the per-row `PENDING`
badge, and the row's post-toggle state.
**Consequence:** A user who toggles on PLUGINS and returns to HOME sees no
reminder there; the PLUGINS tab is the single owner of pending. The right side
of the HOME group line is empty unless a config warning is present.
**Rejected:** keeping a pending badge on HOME; hiding pending entirely.
**Amended by D69:** pending is shown as `NEXT ON`/`NEXT OFF` beside the loaded
status, not as a `PENDING` badge.

## D69 — A row reports the running set, with next boot as a second badge

**Status:** Superseded by D77 (the `OFF` label and the header counts; the
launch-set badge and the three-truth rule survive)
**Context:** The b39 badge was derived from the edited file, so disabling a
plugin painted `DISABLED` while the process still had it loaded. The user could
not tell "written to the file" from "no longer running", which is the one thing
this screen must never blur.
**Decision:** `loaded` is computed from the launch file and never changes during
a session. The status badge reads `LOADED`, `OFF`, `MIXED`, `MISSING`, `NOT
REACHED`, or `BOOT` from that set. A row whose file differs from the launch
pattern adds a yellow `NEXT ON` or `NEXT OFF` badge. The header counts `LOADED`
and `NEXT BOOT`, not `ACTIVATED`. A mixed row disables first, because completing
it adds a load context the confirmation cannot fully describe.
**Consequence:** After a disable the row still says `LOADED` with `NEXT OFF`;
after the reboot it says `OFF`. HOME keeps the launch summary; pending stays on
PLUGINS (D68).
**Rejected:** a file-state badge; one badge carrying both time states; moving
pending back to HOME.

## D70 — Every change is confirmed, receipted, and rebootable

**Status:** Superseded by D72 and D73 (the per-change write, confirmation,
receipt, and session sheet; the battery gate, one-step undo, and `DO NOT REBOOT`
interrupt survive)
**Context:** A Cross press sometimes wrote immediately and sometimes opened a
dialog. A successful write gave no receipt, no next-boot statement, and no way
to reboot or undo. On a screen that edits boot-time system plugins, "it probably
saved" is not good enough.
**Decision:** Every actionable toggle opens a confirmation naming the verb, the
file, the next-boot effect, the row-specific warning, the KERNEL/SYSTEM risk, the
hold-L escape hatch, the line count, and the session change number. Success
opens a receipt that says the file is saved and leaving COCKPIT does not cancel
it. `TRIANGLE` opens `PLUGIN SESSION` with `REBOOT NOW` (when changes wait),
`UNDO LAST CHANGE` (when a snapshot differs), and `CLOSE`. Reboot goes through
`platform_request_reboot()` to `scePowerRequestColdReset()`, is refused below
20 % battery unless charging, warns about the card, and reports a request that
did not reset instead of freezing on a promise. Before every toggle the editor
writes `config.txt.undo` from the verified pre-write bytes and refuses the change
if that snapshot fails. Undo restores it through the same transaction, refuses
if the live file changed on disk, and works when the live file is missing.
`.original` and `.session` are not offered as in-app restore sources.
**Consequence:** A change costs one more press; a reboot costs two deliberate
presses and is never on the same Cross that confirmed a write. Undo is one step,
not a stack, and persists across a relaunch. `.session` is an audit snapshot,
not a promise.
**Rejected:** silent writes; a reboot button on the receipt; in-app `.original`
restore; an in-app EXIT prompt that the PS button would bypass.
**Verification:** `scePowerRequestColdReset` is declared in `psp2/power.h` `[V]`;
that an UNSAFE userland eboot on 3.65 Enso is allowed to cold-reset is `[C]`.

## D71 — Repair the swap, and never silently switch mounts

**Status:** Accepted
**Context:** The rename-away swap has a window where the live file is absent.
A power loss there left only `config.txt.old`, and the next scan called the
config missing while the bytes sat one name away. Separately, an unreadable
`ux0:` config fell back to `ur0:` and then wrote a file the boot never reads. The
final rename failure also reported `RENAME` even when the restore failed and the
live file was gone.
**Decision:** `editor_scan()` calls `platform_recover_swap()` on both config
paths before reading; an absent live file with a surviving `.old` is renamed
back and the panel says `RECOVERED`. A `ux0:` that exists but cannot be read is
read-only and named; neither the editor nor HOME falls through to `ur0:` in that
case. The final rename failure checks the restore and reports `RESTORE` when the
original could not be put back. One over-long path is skipped with a `CONFIG
WARNING` instead of making the whole file read-only. `sceIoSyncByFd` is a
best-effort flush, never a new write failure.
**Consequence:** A power loss in the swap window is recoverable without a PC. An
inert edit to the shadowed mount cannot happen. A failed restore is loud instead
of masquerading as an ordinary rename failure.
**Rejected:** deleting `.old` before the new file is verified; treating a
stat/open failure as permission to switch mounts; failing the whole page on one
long path.

## D72 — Stage the batch, write once

**Status:** Accepted (supersedes D70's per-change write; amends D62)
**Context:** Every switch wrote `config.txt` immediately, so a mistaken press was
already permanent, each change interrupted with its own confirmation and receipt,
and several changes could not be reviewed as one batch. A kernel plugin makes the
risk concrete: one bad entry can stop the console booting. The user asked for a
boundary between intent and the write.
**Decision:** `EditorState.text` is the working set; `last_text` is the file
baseline read at scan and adopted again after a commit. `editor_stage_toggle`
rebuilds the working text from itself, reparses it, and updates the counts with
no platform read, stat, or write. `editor_draft_dirty`, `editor_draft_count`, and
`editor_draft_change` expose the delta. `editor_commit_draft` re-reads the live
file, refuses if it differs from the baseline
(`CONFIG CHANGED ON DISK. RESTART COCKPIT.`), snapshots `.undo` once from the
pre-batch bytes, writes the whole working set once through the verified
transaction, and adopts it as the new baseline. A failed write keeps the draft.
The draft is memory-only: leaving COCKPIT discards it and no draft file is
written. One batch is one undo unit.
**Consequence:** A mistaken press costs nothing until the dock commits, a batch is
one write and one `.undo`, and the launch set cannot move before the reboot.
Costs a 32 KiB working buffer and the stale-file refusal, which asks the user to
restart COCKPIT rather than overwrite a file changed on the card.
**Rejected:** per-switch writes (D70); a persisted draft across app restarts; a
per-line override model that could not re-emit the file byte for byte.

## D73 — The inline dock replaces per-row dialogs and the session sheet

**Status:** Accepted (supersedes D66's reason dialog and D70's confirmation,
receipt, and session paths; implements KD2 and KD10)
**Context:** The confirmation, receipt, and session sheet interrupted the list and
split transaction actions across several surfaces. The user explicitly rejected a
dialog and asked for one place that owns review, discard, save, confirm, restore,
and undo.
**Decision:** A permanent inline dock is pinned to the bottom of the PLUGINS
panel. The list viewport shrinks by the dock height, so the dock never covers a
row. `ui/plugin_flow` derives one `PluginFlowState` (`IDLE`, `DRAFT`, `PREVIEW`,
`SAVED`, `LAST_WRITE`) and owns the action order, labels, availability, warning
precedence, and focus transitions. The safe action is first, the commit is the
rightmost primary, unavailable actions stay dimmed in place and are skipped by
Left/Right. Cross on a row stages it; Cross on the dock activates the focused
action; Triangle toggles dock focus; Down from the last row enters it; Up and
Circle leave it. The old `UI_MODAL_CONFIRM` / `RECEIPT` / `SESSION` / `RESTART`
paths, `plugin_flow_confirm` / `receipt` / `session` / `restart`, and the session
sheet are removed. The unrecoverable-write `DO NOT REBOOT` panel remains the only
interrupt.
**Consequence:** Transaction actions are always visible and never hide the list,
and one module owns copy, order, and availability, so a row cannot promise an
action the dock will not run. Costs a permanent 44-88 px strip and a two-row dock
while a draft or preview is active.
**Rejected:** a focusable footer; a Triangle commit sheet; keeping per-row
confirmation and receipt dialogs.

## D74 — Two restore points, a contextual undo, and a locked-drop refusal

**Status:** Accepted (supersedes D61's "neither is offered as an in-app restore"
and extends D70's one-step undo)
**Context:** Recovery was split between the session sheet's undo and two
snapshots that only a PC could use. Separately, a snapshot could omit
`henkaku.suprx` or a BOOT module and turn a recovery action into a boot risk.
**Decision:** `.original` and `.session` are readable restore sources and are
previewed as a plugin-level diff against the current working set (the draft when
one exists): `+` / `-`, `WILL ENABLE`, `WILL DISABLE`, `WILL REMOVE`, unchanged
rows dim. The whole snapshot replaces the file, including comments and sections;
no line-level diff is offered. `UNDO LAST WRITE` previews `.undo` under the same
confirm/cancel rules. A restore snapshots the pre-restore file to `.undo` before
writing when a live file exists; an undo preview writes the `.undo` bytes it
already read without replacing `.undo`; with no live config the undo skips the
snapshot. `.undo` is removed only after a verified undo. A restore that would remove or disable a BOOT
module or a locked HENKAKU path is refused with the path named, and a snapshot
that matches the working set reports `NO CHANGES - NOTHING TO RESTORE`; neither
can be confirmed or saved.
**Consequence:** Two stable recovery points plus one contextual step, and a
dangerous restore cannot be confirmed by accident. Costs a preview buffer and the
whole-file replacement: comments and sections come from the snapshot, not from a
merge.
**Rejected:** a third permanent restore point; a line-level diff; allowing a
restore to drop a locked or boot path.

## D75 — Save without reboot is a first-class action

**Status:** Accepted
**Context:** `CONFIRM & REBOOT` was the only way to apply a staged batch. An
advanced user who wants to prepare the next boot without restarting had to
pretend the change was live or skip it.
**Decision:** `SAVE, REBOOT LATER` writes the batch, clears the draft, and leaves
`SAVED - REBOOT TO APPLY`; `REBOOT NOW` applies it. Its warning is `SAVE ONLY -
NOTHING CHANGES UNTIL THE VITA RESTARTS / THE PLUGINS RUNNING NOW STAY LOADED.`
The battery gate covers both `CONFIRM & REBOOT` and `REBOOT NOW`, and the save
path becomes the primary action when a reboot is blocked. After a relaunch the
dock shows `LAST WRITE - UNDO AVAILABLE` when `.undo` differs, not a false
`SAVED` banner, because the app cannot prove whether the saved config booted.
**Consequence:** The file can be prepared now and applied later without claiming
the running set changed. Costs the saved/last-write truth table and a documented
ambiguity after a relaunch or reboot.
**Rejected:** confirm-and-reboot only; presenting a saved file as a running one.

## D76 — Sticks scroll; the D-pad chooses

**Status:** Accepted (supersedes D45's shared D-pad/stick scroll)
**Context:** The stick was mapped to discrete up/down, so it fought the D-pad for
selection and could not scroll a list smoothly. The user asked for continuous
scroll and discrete choice to stop being the same input.
**Decision:** `InputState` carries a per-frame `scroll_y`; the app reads both
sticks, takes the larger vertical deflection, applies a 0.18 dead zone, squares
the travel after the dead zone, and caps it at 520 px/s. The result is clamped
and applied to the active surface: the HOME canvas, the PLUGINS list, or the
restore preview. The D-pad keeps its edge and repeat events and moves the row
selection or dock choice. D-pad Left/Right are their own `INPUT_LEFT` /
`INPUT_RIGHT` events for the dock; L/R stay tab switches. HOME keeps a stepped
D-pad scroll because it has no selection. On PLUGINS the stick is the only way
past the last row into the orphan section; Down at the last row enters the dock.
Modals ignore the stick. The old digital stick-to-up/down mapping is removed.
**Consequence:** Scrolling and selection are independent, and the preview can
scroll without a cursor. Costs a second input channel and hardware tuning of the
feel.
**Rejected:** the stick as a digital D-pad; stick scrolling only on PLUGINS.

## D77 — The header has three slots and reports the working set

**Status:** Accepted (supersedes D69's header counts and `OFF` label)
**Context:** The header carried `ROWS / LOADED / NEXT BOOT`, a `CROSS CHANGE`
hint, and an undo-status token. It described a blend of the launch set and the
file, and a read-only reason replaced the state line instead of having a reserved
slot. The user asked for `USING`, `DRAFT`, the counts, and a stable exception
slot.
**Decision:** Three 14 px line slots, no fourth line. The source line renders
`USING` dim, the path cyan, `DRAFT` or `PREVIEW` yellow, ACTIVE green with a dim
label, DISABLED red with a dim label, dim pipes, and an optional yellow `NEXT
BOOT`. ACTIVE counts working-set rows with at least one reached active line;
DISABLED counts the rest; NEXT BOOT counts working-set rows that differ from
launch and appears only when nonzero. The second slot is blank or carries one
yellow pipe-separated exception line, in precedence order: read-only reason,
config warning, backup warning, shadowed path, recovered; it appends only while
it fits and truncates the last segment with `..`, without changing the header
height. The third slot renders `PRESS`, a 14 × 12 keycap with a white outline
triangle, and `FOR SESSION OPTIONS`. `OFF` becomes `NOT LOADED`; the row badges
are `LOADED`, `NOT LOADED`, `MIXED`, `BOOT`, `MISSING`, `NOT REACHED`, plus
`LOCKED` and `NEXT ON` / `NEXT OFF`. The old `ROWS / LOADED / NEXT BOOT`,
`CROSS CHANGE`, and undo-status text are removed.
**Consequence:** The ledger never shifts under a warning, and the header names
the working set while the badge keeps the launch set. Costs the exception line's
truncation and loses the header's undo hint, which now lives in the dock.
**Rejected:** a conditional fourth line; keeping `OFF`; one blended status.

## D78 — Red is a narrow off token; green dims active titles

**Status:** Accepted (supersedes D40; amends D19 for PLUGINS status counts)
**Context:** D40 removed red after it had become decoration. The split rocker now
needs a legible off state, the `NOT LOADED` badge needs to read as absence, and
an active title needs to read as a state without competing with the lit control.
The user selected the dim-title and limited-red variants.
**Decision:** `C_RED` `#FF2E4D` is limited to the DISABLED count, the dock's
DISABLING count, the rocker's off half, and the `NOT LOADED` badge. It never
colours a label, a title, an action, or an alert. `C_GREEN` `#3BE07A` is the
rocker's on half, the `LOADED` badge, the ACTIVE and ENABLING counts, and a
`WILL ENABLE` marker. `C_GREEN_DIM` `#2FA85C` colours a plugin title active in
the working set; an inactive title stays white. `C_CYAN` is the path, `C_YELLOW` is
`DRAFT`, `PREVIEW`, pending, exceptions, and the primary dock action, `C_WHITE` is
the keycap triangle and the off title, and `C_DIM` is labels, pipes, and secondary
actions. Colour is never the only cue: badge text, rocker half position, the
selection wash and bar, and the draft notch carry the state too. The three truths
stay separate: the badge is the launch set, the title and rocker are the working
set, and `NEXT ON` / `NEXT OFF` is the next boot; colour supports those shapes,
it never replaces them. The "numbers are always white" rule stays true for metric
readouts; the PLUGINS counts are status tokens, not readouts.
**Consequence:** Red returns, but only as an off token, so the hazard band is
still the only alert. Green no longer means only "loaded": it also means on. The
palette is wider and the limited-red rule must be checked in review.
**Rejected:** unrestricted red; bright green titles; a blended neutral that would
make the three truths indistinguishable without reading every label.
**Verification:** `[C]` the contrast of `C_GREEN_DIM` and `C_RED` on the panel
fill is not measured on hardware; the host preview renders the chosen values.

## D79 — A whole-file restore requires a readable, fully parsed live file

**Status:** Accepted (clarifies D70 and D71)
**Context:** The restore previews read `.original` and `.session` and write the
snapshot whole. D70 says a restore takes the current file as `.undo` first; D71
says an unreadable config is read-only and must not be silently replaced. A
review found that the restore path skipped both rules: `had_live` was false for
an unreadable file, so the write destroyed bytes the app had never read and took
no `.undo`; an oversized file could never satisfy the stale check and every
restore ended in `CONFIG CHANGED ON DISK`. A `*!` halt before HENKAKU also
bypassed the locked-drop check because the row was unreached rather than
disabled.
**Decision:** An ORIGINAL or SESSION restore is offered only when the live config
was read and modelled completely: not unreadable, not oversized, and not
truncated by the line or row ceiling. Otherwise the restore chips are dim, the
idle dock names the reason, and the user edits the file outside COCKPIT. Undo is
the exception: it repairs an absent, unreadable, or partially replaced file, so
it stays available and, after an unrecovered write, its preview is always
committable even when the in-memory baseline equals the snapshot. The protected
drop test keys on `active_lines == 0`, which covers both a fully commented path
and one behind `*!`. A failed save preserves the previous `.undo`; a failed
undo removal keeps the surviving `.undo` reported.
**Consequence:** A config the app cannot parse is repaired through VitaShell or
UNDO, not through a blind whole-file replace. The restore path is narrower, but
every write it performs has a readable baseline and a real recovery point.
**Rejected:** restoring over an unreadable file with an empty `.undo`; treating
an oversized file as restorable by skipping the stale check; an override that
writes without a pre-restore snapshot.
**Verification:** `[V]` host tests cover the unreadable refusal, the undo repair,
the `*!` halt, the removed-path pending count, the failed-save `.undo`
preservation, and the failed-removal flag. `[C]` the hardware checklist must
confirm the read-only header, the dim restore chips, and the undo repair on the
console; no hardware evidence exists yet.

## D80 — Navigation and the config-write protocol are testable modules

**Status:** Accepted (implements the deferred review follow-ups)
**Context:** The code review found two behavior-bearing policies that only the
console could exercise: `app.c` owned the tab/list/dock event routing and the
SAVE-versus-CONFIRM decision, and the host suite never links `app.c`; and the
only config write existed twice, once in the draft commit and once in the
restore commit, with the copies already drifting. It also found the dock's
height, title, action list, and primary count spread over parallel switches, and
the editor's launch/file-row cache and preview scratch exposed in the header.
**Decision:** `ui/plugin_nav` owns the event-to-action mapping, the
SAVE-vs-CONFIRM commit mode, and the scroll clamps; `app.c` keeps the pad, the
clock, and the execution of the action. `core/plugin_editor` has one
`commit_text` helper for the stale check, the optional `.undo`, the verified
write, the failure mapping, and the baseline adoption; the public draft and
preview commits keep their own guards and post-commit meaning. `ui/plugin_flow`
holds one `FlowSpec` table for the state's height, title, actions, and primary
count, with SAVED as the documented undo-dependent exception. The preview
scratch and the launch/file-row cache are file-private in `plugin_editor.c`; the
header exposes the launch lookup the host test needs.
**Consequence:** The host suite can exercise the routing and both commit paths
without a Vita, and the next write-protocol change has one place to make. The
modules are smaller and the header no longer carries the cache arrays.
**Rejected:** leaving the routing in `app.c` and testing it only on hardware;
copying the write protocol into a third restore path; deriving the dock facts
from the panel's layout if-chain.
**Verification:** `[V]` host tests cover the event routing, the SAVE/CONFIRM
mapping, the scroll clamps, and the draft/restore/undo success and failure paths
through the shared protocol. The preview renders the same 38 scenes and the
busiest frame is unchanged. `[C]` the refactor has not run on hardware; the
write/restore/undo checklist still applies to the resulting build.

## D81 — FRAME is HOME-only; the dock is a row of pressable slabs

**Status:** Accepted (supersedes D45's pinned strip for PLUGINS and the dock
heights in D72/D75)
**Context:** FRAME measured the renderer on every tab, including PLUGINS, where
it competed with the transaction dock for the bottom of the screen. The dock
itself was a row of 20 px text chips at micro scale; the user read it as cramped
and not pressable. The PLUGINS tab needs the bottom of the screen for the
transaction, not for frame health.
**Decision:** FRAME is drawn on HOME only. PLUGINS uses a 472 px band
(`PLUGINS_VIEW_H`) and an 8 px bottom margin; the ledger and dock reach screen
y 536. The dock has one height per state: idle 64, draft 152, preview 114,
saved/last-write 106. Actions are 34 px primary and 28 px secondary slabs with a
top highlight, a bottom ledge, a 2 px white focus outline, and label-scale text;
the primary is a solid yellow slab with black text. The title and actions use
px 2; the summary and warnings use px 1.5. Warnings get their own bottom line
behind a 2 px yellow marker; an idle restore failure draws its reason beside the
restore buttons behind the same marker.
**Consequence:** The bottom of PLUGINS is a transaction surface with clearly
pressable controls, and frame health is no longer duplicated there. The dock is
taller, but removing the frame strip and extending the band keeps the visible
list at roughly the same depth.
**Rejected:** shrinking the chips further; keeping FRAME on every tab; a dialog
or a single-row toolbar that hides the transaction copy.
**Verification:** `[V]` the host preview renders every dock state at the new
heights; the busiest scene is 5,514/8,000 draw calls. `[C]` the press feel and
the 8 px bottom margin are not measured on hardware; the checklist covers the
dock flow.

## D82 — CORE shows applied clocks through an on-demand kernel reader

**Status:** Superseded by D84. The syscall import described here was bound to
address zero on the 3.65 console and crashed b74 at startup; see D84 for the
evidence and the hook replacement.
**Context:** PSVshellPlus sets the GPU from 111 to 222 through the kernel and
blocks per-process clock changes. COCKPIT kept reading 111 because
`scePowerGetGpuClockFrequency()` reports the calling process's clock **request**,
not the applied clock; the same is true of the ARM and bus getters. The original
PSVshell hides this by hooking the userland getters, but PSVshellPlus does not.
The result was a real number that was not the machine's current clock, which
breaks the product rule that every number is real.
**Decision:** CORE reads the applied clocks from a small read-only kernel module,
`kernel/clock_reader.c`, shipped inside the VPK as `cockpit_clocks.skprx`. The
module resolves `kscePowerGetArmClockFrequency`,
`kscePowerGetGpuClockFrequencyInternal`, and `kscePowerGetBusClockFrequency`
from the ScePowerForDriver exports at runtime and refuses to start if any is
missing. The composition root loads it from `app0:` with
`taiLoadStartKernelModule()` before the first sample, and the adapter calls it
every 100 ms. It is never installed in `*KERNEL`,
never written to `boot_config.txt`, and never loaded by the safe build. If the
module is absent or a read fails, the clock row renders `--`; there is no
userland-request fallback. The app links the weak stub so a missing module
cannot stop the eboot from loading, and the unresolved stub returns an error
that the adapter treats as absent.
**Consequence:** The clock row now reflects an overclock applied by
PSVshellPlus or the original PSVshell. The VPK carries 4.7 KB of kernel code and
the default build links taiHEN; the safe build stays a pure userland eboot and
shows `--` for clocks. The on-demand loader and the applied values are `[C]`
until the console checklist runs. `kscePowerGetArmClockFrequency` does not
report the 500 MHz PLL override, so a 500 MHz ARM setting can still read 444;
the module reports what the kernel reports and never guesses 500.
**Rejected:** keeping the userland getters and relabelling them `REQ`; parsing
PSVshellPlus profiles (a request, not an applied value); making COCKPIT depend
on PSVshellPlus's private exports; installing the reader in `*KERNEL` (bootloop
risk and a boot-config write).
**Verification:** `[V]` the ARM build produces `cockpit_clocks.skprx` and the
VPK contains it; the host tests cover the clock port's valid and absent paths;
the preview renders the `--` state. `[C]` the module load, the syscall, and the
applied values have not run on hardware.

## D83 — Off is the left rocker half; the dock gets 12 px top padding

**Status:** Accepted (supersedes the dock heights in D81 and the left/right
mapping in D77's rocker description)
**Context:** The split rocker put ON on the left and OFF on the right, the
opposite of the conventional switch direction the user expects. The dock's
state block started 8 px below the top edge while the warning line left about
10 px below it (and the saved state left more when the warning was empty), so the
panel read as cramped at the top.
**Decision:** The left half is OFF (`C_RED`) and the right half is ON
(`C_GREEN`); a mixed row still lights both, and the draft notch stays on the
right edge. The dock's inner top padding is 12 px and the warning line leaves
12 px below it. Heights become idle 64, draft 159, preview 123, saved/last-write
111. The idle strip stays 64 with 15 px above and below its buttons. Button
heights, the yellow primary, the dark secondary ledges, and the white focus
outline are unchanged.
**Consequence:** The switch now reads the way a physical toggle does, and the
draft/preview/saved docks have equal air above and below their content. The
taller dock costs the list 5-9 px depending on state. The saved state still
reserves the warning line, so an empty warning leaves that line's height empty
below the buttons rather than reflowing the dock.
**Rejected:** a true sliding-knob animation (the split rocker is the established
control vocabulary); keeping the old left-on mapping; adding outer space above
the dock instead of inner padding; changing the dock height when the warning is
empty (it would make the list jump).
**Verification:** `[V]` the host preview renders every dock state at the new
heights and the rocker in both directions; the busiest scene is 5,514/8,000 draw
calls. `[C]` the press feel and the 12 px optical balance are not measured on
hardware; the checklist covers them.

## D84 — The applied-clock reader hooks the ScePower getters; no syscall import

**Status:** Superseded by D85
**Context:** b74 shipped D82's on-demand `.skprx` with a generated userland stub
and a weak import. On the 3.65 console b74 crashed at launch with C2-12828-1.
The core `psp2core-1793774060-0x0000a23b8f-eboot.bin.psp2dmp.tmp` names the main
thread `COCKPIT01`, stop reason `0x30003` (prefetch abort), PC `0`, and r0
`0x812c0ae4`. That r0 is `&reading` in `platform_read_clocks`, and the call site
is `cockpit_clocks_read` at `platform_read_clocks+0x2e`. A weak import from a
module that is not loaded when the process starts is bound to address zero on
this firmware; the first slow sample called it and jumped to null. The load
call's result did not matter: the import is resolved before the module exists.
**Decision:** Drop the syscall library, the generated stub, and the weak import.
The module now hooks `scePowerGetArmClockFrequency`,
`scePowerGetGpuClockFrequency`, `scePowerGetBusClockFrequency`, and
`scePowerGetGpuXbarClockFrequency` in the ScePower userland library (library
NID `0x1082DA7F`) with `taiHookFunctionExportForKernel(KERNEL_PID, ...)`, the
same mechanism the original PSVshell uses. The hooks call the
ScePowerForDriver getters and return applied values. The app uses the ordinary
getters. The XBAR getter is the handshake: COCKPIT calls it through a
one-argument function pointer with `0x434F434B`; the hook returns `0x4B434F43`
for that key and passes every other caller through to the real getter. If the
handshake does not match, CORE renders `--` and does not read the clock getters.
The module is still loaded on demand from `app0:`, is never in `*KERNEL`, and
never touches `boot_config.txt`.
**Consequence:** There is no load-order dependency: a failed or absent module
leaves the app running and shows `--`. The hooks are global while the module is
loaded, so other callers see applied clocks too (as they do with PSVshell); the
module stays loaded until reboot, and a later COCKPIT launch still passes the
handshake. The safe build neither ships nor loads the module and shows `--`. The
VPK carries 5.2 KB of kernel code. The 500 MHz ARM PLL override can still read
444 because ScePower does not report it.
**Rejected:** keeping the weak import and guarding the call (the import is
already zero); installing the module in `*KERNEL` (forbidden, bootloop risk);
making COCKPIT depend on PSVshellPlus; parsing its profile; showing the request
as the applied clock; a separate plugin install.
**Verification:** `[V]` the default build no longer imports `CockpitClocks`
(only `taihenUnsafe`), the VPK contains `cockpit_clocks.skprx` (5,202 bytes),
the module's import table resolves the taiHEN hook and module-utils functions,
and the host tests and 39 previews pass. `[C]` the hook installation and the
applied values have not run on hardware; the checklist covers them.

## D85 — Applied clocks come from an app-local PSVshellPlus bridge

**Status:** Accepted (supersedes D82 and D84); superseded by D99 (2026-11-05)
**Context:** The on-demand kernel path is shell-gated: taiHEN's
`taiLoadStartKernelModuleForUser` checks `ksceSblACMgrIsShell(0)`, so an app
cannot install the reader it ships. b79 wrote the load result to
`ux0:data/cockpit-clocks.txt`; that diagnostic is retired with the path. The app
still must not show the userland clock request as the applied clock, and it must
work when PSVshellPlus is absent.
**Decision:** Ship a small app-local `.suprx` at
`system-plugin/clock_bridge/`. `platform_init()` loads
`app0:system-plugin/cockpit_clock_bridge.suprx` once and passes a pointer to the
app's static `CockpitClockState`. The bridge hard-imports
`psvsGetClockFrequency` from `PSVshellPlus_KernelForUser` (library NID
`0x0366B343`, function NID `0x3A7A3A18`), reads it every 100 ms, and publishes
ARM/GPU/BUS under a seqlock. `platform_read_clocks` accepts only a complete
sample; a missing library, refused load, torn read, or invalid sample is `--`.
The bridge does not call `psvsSetClockingPid`. It is never installed to `ur0:` or
`*KERNEL`; the safe build neither ships nor loads it. This release installs no
COCKPIT kernel component.
**Consequence:** No kernel code and no boot-time load. PSVshellPlus, when
present, is the clock source; without it CORE shows `--`. A 500 MHz per-app
request may read 444 because the bridge reads the kernel-applied value for the
default clocking pid; that is the applied value, not a fabricated request. The
bridge lives and dies with the app process, so there is no app-side unload call.
**Rejected:** keeping the on-demand kernel reader (shell-gated); a weak import
from PSVshellPlus in the app (b74's PC-0 crash); a direct hard import in the app
(it would make COCKPIT fail without PSVshellPlus); a userland request fallback;
installing the bridge in `*KERNEL`.
**Verification:** `[V]` the default build produces the bridge (5,049 bytes) and
the VPK carries it under `system-plugin/`; the safe VPK carries none; the app
imports neither `CockpitClocks` nor `PSVshellPlus_KernelForUser`; the bridge's
import table resolves `psvsGetClockFrequency`; host tests cover the seqlock and
the one-shot load. `[C]` the unsigned `app0:` load and the applied values have
not run on hardware; the checklist covers them.

## D86 — Install, remove, and conflict edits use private editor batches

**Status:** Accepted
**Context:** The PLUGINS dock is a user draft: stage, discard, save, reboot. The
system plugin needs config edits that the user did not stage row by row, and a
second writer or a mixed draft would break the single-writer rule. A failed
system batch must not become a committable dock draft.
**Decision:** Add `editor_stage_insert`, `editor_stage_remove`,
`editor_stage_disable`, and `editor_stage_enable`, plus
`editor_commit_system`. They share the editor's line model and verified write.
Insert goes inside `*main`'s path run before the next header, creates `*main`
when absent, and refuses across `*!`. A private batch refuses a pre-existing
user draft and is cleared on every failure path. The COCKPIT self row stays a
real row but is hidden from the ledger counts and visible walk. Restore previews
flag self removal separately from the locked-path refusal and flag an add with
missing payload as REPAIR. REMOVE commits without creating a new `.undo`, and a
surviving `.undo` is reported before any payload file is deleted.
**Consequence:** One editor and one verified write remain the only config path.
The dock never shows a system batch; a failed install leaves the old payload and
no committable draft. A conflict restore can join the remove batch. The user
must discard or commit a draft before a system action.
**Rejected:** a second config writer; mixing a system batch into the dock; a
rescan after the commit; deleting payload files while a restorable `.undo`
survives.
**Verification:** `[V]` host tests cover `*main` insertion and creation, `*!`
refusal, BOM/CRLF, duplicate self lines, self-row hiding, self removal/repair
previews, a failed batch clearing, and `.undo` removal. `[C]` the SDK rename
semantics remain unverified on hardware.

## D87 — System-plugin state is orthogonal facts with a manifest and a probe

**Status:** Accepted
**Context:** A single state enum cannot express "installed but not rebooted",
"inactive update", "resident older than disk", "remove committed but not
rebooted", and an update overlay without contradictions. The status file is
advisory and lives under an FTP-exported tree.
**Decision:** `core/system_plugin` derives the display state from payload,
manifest, entry, resident/ack, panel, pending action, and overlay facts with a
precedence table. The plugin version is a source-plus-toolchain hash carried in
`plugin.manifest` with hashes and a build number; a fresh install before reboot is
REBOOT REQUIRED, resident ≠ on-disk is REBOOT REQUIRED only when active, an
inactive update needs no reboot, an app older than the payload is REPAIR with no
downgrade, and a missing/corrupt manifest is REPAIR. Status is trusted only with
a matching nonce/ack and `panel=1`; a stop needs a matching ack plus a closed
loopback probe, and an inconclusive probe requires the force confirmation.
Install stages the editor batch before copying, verifies the pair and manifest,
backs up the old pair, and restores it on failure. Remove commits config and
`.undo`, then deletes files. The prompt and conflict records live under
`ux0:data/COCKPIT/`, outside REMOVE's deletion.
**Consequence:** The block can say `--`, `CHECKING`, `REPAIR`, or `REMOVE
INCOMPLETE` honestly instead of guessing. The probe is an independent check, not
a security boundary. A manifest hash is copy integrity, not a signature. The
force path ends in POWER OFF NOW because a config entry is already gone while
the server may still be live.
**Rejected:** a single enum; file existence as WORKING; an ack without a probe;
copying an older payload over a newer install; a separate cleanup action; a
battery-gated reboot after a forced action.
**Verification:** `[V]` 404 host checks cover the precedence table, stage-before-
copy, second-file rollback, remove ordering, `.undo` survival, conflict
restoration, status spoofing, probe-gated stops, force power-off, partial
states, and the safe refusal. `[C]` the probe and the shell-side status writer
have not run on hardware.

## D88 — The PAF shell is built from a copied SDK with a build-time import audit

**Status:** Accepted
**Context:** The `*main` shell plugin needs the PAF C++ headers and stubs that
the stock VitaSDK does not ship. The PAF component's installer deletes and
replaces `ScePaf` files inside whatever `VITASDK` it is given, and it assumes
Linux/WSL2 host tools. Mutating the app's SDK would make the app build depend on
a component it never uses, and a weak PAF stub that resolves to zero would
repeat b74's failure class inside SceShell.
**Decision:** `setup-paf.sh` copies the stock SDK to a cache outside the
repository and installs the MIT PAF component, the GPL-3.0 RCO compiler, and
`libftpvita` there. Host-only `-Wl,-q` and `nproc` assumptions are repaired in
the cache, never in the source tree. The shell links strong PAF stubs and
`SceLibc`, defines the SN C `__declspec` shim in `compat.h`, and passes
`import-audit.sh`; a `_stub_weak` link or unresolved import fails the build.
ScePaf-RE is fetched by the component's installer and is not vendored.
**Consequence:** The app and bridge remain on the stock SDK, and the default
VPK carries the shell, RCO, and manifest as app0 payload. `--safe` does not
require the PAF tree. The shell now carries the status endpoint and FTP worker; the import audit
proves resolution, not runtime behaviour.
**Rejected:** mutating the shared `vitasdk/`; DolceSDK (no installable
package index); weak PAF stubs; vendoring ScePaf-RE; a runtime import audit
(a hard import that fails resolution prevents `module_start` from running).
**Verification:** `[V]` the shell and RCO build, the default VPK contains
`cockpit_system.suprx` (23,628 bytes), `cockpit_plugin.rco` (1,168 bytes), and
`plugin.manifest` (79 bytes), the safe VPK contains none, and the import audit
reports 85 imports with no unresolved required or weak stub. `[C]` the SceShell
hooks and panel have not run on hardware.

## D89 — The FTP worker owns state; the control thread owns status.txt

**Status:** Accepted
**Context:** The Quick Menu toggle, the app's stop request, and the network can
all change FTP state from different threads. Two writers of `status.txt` or of
the listener flags would let the panel claim ON while 1337 is closed, or OFF
while the server socket is still bound. `libftpvita` also assumed a
single-client, never-stopping process: it joined client threads while holding
the client-list mutex, reset its device table on every init, and treated port
errors as an async thread failure.
**Decision:** The worker thread exclusively owns `FtpContext` and publishes an
`FtpSnapshot` under a mutex. The menu and the control thread only write the
intent word. The control thread consumes `control.txt`, polls the snapshot,
and is the sole `status.txt` writer. A stop clears intent, waits for the
listener to close, then acknowledges; the app still probes 1337. Intent-off
never clears `server_up`, so the worker always sees a live listener and calls
`ftpvita_fini`. The forked `libftpvita` binds synchronously, returns distinct
init errors, caps clients at eight, aborts and joins outside the client-list
mutex, and exposes a loud accept-loop-dead signal. Devices are registered after
each successful init because init clears the table.
**Consequence:** The panel and status can only report a snapshot the worker
actually published. A client mid-transfer can delay `ftpvita_fini`, so the
control stop waits at most one second and leaves the final decision to the
probe. `s_pub_mutex` is retained across module stop to protect an in-flight
panel callback. The eight-client cap bounds transfer buffers at 2 MiB.
**Rejected:** letting the worker call `shell_control_set_ftp` (re-entered the
control mutex and made STOP deadlock); clearing `server_up` when intent goes
off (left 1337 bound); deleting `control.txt` after parsing only (replayed the
same stop); joining client threads under `client_list_mtx` (deadlocked on a
peer-closed client); rebuilding the device table once at worker start (init
wiped it).
**Verification:** `[V]` 52 shell-core checks include the intent/server split and
snapshot copy; 2,077 host checks pass; the patched library applies to a clean
pinned checkout and the shell's import audit resolves 85 imports. `[C]` the PAF
hooks, panel, worker, 1337 probe, and real client behaviour have not run on
hardware.

## D90 — The self-plugin block owns its actions; choice modals are one row

**Status:** Accepted
**Context:** Install, update, repair, remove, disable, and the force escape have
different safety rules. Spreading them across the ledger, the dock, and several
dialogs would let the hidden self row and the visible block disagree, and a
nested menu would hide the consequence the user is choosing. The first launch
needs one disclosure that fits the existing modal, not a second screen.
**Decision:** The COCKPIT self row stays a real editor row but is hidden from the
ledger, selection walk, group counts, and HOME totals. A fixed 72 px block
between the header and the first group owns its state label, FTP hint, and
ordered actions. Up from the first visible row enters the block; Left/Right
moves the action cursor; Cross runs it; Circle/Down returns to the list;
Triangle stays dock-only. ENABLE and DISABLE stage through the editor draft;
the dock hides `CONFIRM & REBOOT` for a self-only batch, so the block's REBOOT
is the single reboot surface. Install, conflict, remove, force, power-off, and
update-failed use one-row choice modals with a non-destructive default.
The stop gate runs two frame-paced ack-plus-probe requests; a refused 1337 can
also end a stop when the shell is gone, so a deleted payload cannot turn every
REMOVE into a force power-off. A real failure offers the confirmed force path,
which commits outside the dock and opens `POWER OFF NOW`. Normal reboots use the
existing battery gate. The safe build hides the block and the first-launch
prompt, and a launch-time update copies without a prompt.
**Consequence:** The block and the dock cannot both claim the self reboot. A
broken plugin still has a visible REMOVE and a confirmed force escape, and the
app never fabricates an FTP address without a matching ack. A restore or undo
that removes the self entry runs the same gate through the dock path; a failed
stop aborts that commit and leaves the force action available. `POWER OFF NOW`
requests standby because the SDK exports no power-off call; the modal tells the
user to hold power if standby returns.
**Rejected:** a fourth header line; the self row in the ledger; a nested action
menu; an ack-only stop; a battery-gated reboot after a forced action; starting
FTP automatically after a discard; inventing an IP for an ON status with no
trusted address.
**Verification:** `[V]` 2,077 host checks include the block action table, every
modal label/command/default, block and modal navigation, the hidden self row,
and the input context's Left/Right modal routing; 47 preview scenes cover the
block states and the install/conflict/remove/power-off modals, busiest 6,109 of
8,000 draw calls; default and safe builds pass. `[C]` the first-launch prompt,
install/update/remove writes, stop/ack/probe, standby escape, and block drawing
have not run on hardware.

## D91 — A directory is tested with `st_mode`, and its parents are created

**Status:** Accepted (supersedes the install adapter assumption in D90)
**Context:** The 2026-11-04 b124 console run installed nothing. `platform_make_dir("ur0:tai")` called `sceIoMkdir`, got `EEXIST` because Enso owns the directory, then tested `SCE_SO_ISDIR(st.st_mode)`. `SCE_SO_*` describes `st_attr`; `st_mode` uses `SCE_S_*`. The predicate is false for every real directory (`0x11FF & 0x38 != 0x10`), so the adapter returned -1, `stage_payloads` stopped at `MKDIR`, and no temp, payload, config line, or marker was written.
**Decision:** Every stat site uses `SCE_S_ISDIR(st_mode)` / `SCE_S_ISREG`. `_Static_assert`s in the adapter pin the distinction. The install creates `ur0:data` before `ur0:data/COCKPIT` and `ux0:data` before `ux0:data/COCKPIT`; `sceIoMkdir` creates one level. A rename that fails for a new destination falls back to copy-then-delete, because a first install has no target to remove and rename. A copy whose `sceIoGetstat` fails retries with `sceIoOpen` instead of reporting the shipped payload MISSING.
**Consequence:** An existing `ur0:tai` is a no-op, not a fatal error, and a clean console without `ur0:data` can install. The adapter now also classifies directories correctly for list, remove, and file-exists, so a non-empty `ur0:data/COCKPIT` reports DELETE FAILED instead of a false success.
**Rejected:** creating the two directories only at first launch; treating `EEXIST` as success without checking the type; keeping the rename-only replace; using `SCE_SO_*` with `st_attr` (the filesystem APIs here return `st_mode`).
**Verification:** `[V]` the SDK header defines the two families separately and the static asserts compile; 2,142 host checks pass. `[C]` the real mkdir/rename on this 3.65 unit is confirmed only by the b141 checklist.

## D92 — The shell declares its entry point and does not import SceLibc

**Status:** Accepted (supersedes the U4 shell link flags)
**Context:** The b124 `cockpit_system.suprx` exported NID `0x935CD196` (`module_start`) with address `0x81000000`, which was `cockpit_check_cb`; the real `module_start` sat at `0x810007f4`. The shell linked `-nostartfiles -nostdlib` without `-Wl,-e,module_start`, so the linker defaulted `e_entry` to `.text` and `vita-elf-create` exported that address. The loader calls the exported address, so the plugin could never start even with correct files. The same binary hard-imported 14 `SceLibc` functions into a `*main` SceShell module, which the plan forbids because SceShell does not guarantee libc and one unresolved hard import prevents `module_start` from running.
**Decision:** The shell links `-Wl,-e,module_start` and passes `MODULE_ENTRY "module_start,module_stop,module_exit"` to `vita_create_self`; `vita-elf-create` needs both to export all three entry addresses. `port/paf_libc.c` provides the C names the FTP library needs on top of the PAF heap and `vsnprintf` (D98 makes the `str*`/`mem*` set local), and `SceLibc_stub` is no longer linked. `strstr` is local because the PAF component's stub maps that name to a NID the retail database calls `sce_paf_private_strtok`. `core/mini_sscanf.c` implements the small `%d`, `%u`, `%c`, `%s`, `%[...]` subset the FTP grammar uses; it is host-tested. The import audit parses the export table and fails when any of the three entry addresses differs from its symbol, or when the ELF contains an `SceLibc` import.
**Consequence:** The shipped module can start and unload in SceShell without a libc provider. A future entry-point regression fails the build instead of producing an inert plugin. `sprintf` keeps unbounded semantics through `sce_paf_vsnprintf`; the FTP call sites own their buffers.
**Rejected:** `MODULE_ENTRY` alone without `-e` (the linker entry would still be `.text`); `-e` alone (stop/exit stay unexported); keeping weak libc stubs (they resolve to zero at load); assuming SceShell has libc because the app does; forwarding `strstr` to the PAF NID whose retail name is a tokenizer.
**Verification:** `[V]` the rebuilt velf pairs `module_start`/`module_stop`/`module_exit` with `0x81000c2c`/`0x81000f04`/`0x81000f88`, matching `nm`; the audit reports 70 imports with no unresolved or `SceLibc` entry; 73 shell-core checks pass. `[C]` whether SceShell loads this module and creates the panel is the b141 hardware item.

## D93 — Install and reboot failures are modals; the install prompt opens on its action

**Status:** Accepted (supersedes the "non-destructive default" part of D90 for INSTALL and CONFLICT)
**Context:** `do_install` closed the modal before checking `plan.ok` and wrote the error to `ui->dock_message`, which only the PLUGINS screen draws. The first-launch prompt is opened from HOME, so every refusal looked like a dialog that vanished. Separately, `UI_MODAL_INSTALL` focused `NOT NOW` and `UI_MODAL_CONFLICT` focused `CANCEL`; a plain Cross on the first-launch prompt wrote `prompted=1`, suppressed the prompt forever, and never ran the install.
**Decision:** A failed install opens `INSTALL FAILED` with the stage and error. A gated or refused reboot opens `REBOOT BLOCKED` / `RESTART FAILED`. `INSTALL` and `CONFLICT` default to their primary action; REMOVE, FORCE_DISABLE, FORCE_REMOVE, POWER_OFF, and UPDATE_FAILED keep the cancel default. Cancelling the conflict chooser no longer writes the not-now marker. With no ledger row selected, Cross runs the block's action instead of silently doing nothing.
**Consequence:** A refusal is readable on the tab where the user started, and the prompt cannot be dismissed into a permanent no-op by a single Cross. A read-only or missing `config.txt` is named before the install modal opens instead of failing later at `SCAN REQUIRED`.
**Rejected:** drawing the dock on HOME (it is a PLUGINS commit surface); keeping NOT NOW focused and adding a focus legend only; writing the marker from the conflict cancel path.
**Verification:** `[V]` 265 UI checks include the new defaults and the empty-ledger block route; the install-failure modal is exercised by the app build, not the host harness (app.c is not host-linked). `[C]` the on-console modal and block input are b141 checklist items.

## D94 — The stop gate writes once and polls across frames

**Status:** Accepted
**Context:** The shell removes `control.txt`, waits up to 1 s for the listener to close, then writes the stop ack. The app wrote `stop` and read the status once, retried at 400 ms, and offered force after two attempts. The ack commonly landed after the second read, so a normal disable or remove fell to `POWER OFF NOW` while the listener had actually closed.
**Decision:** `system_plugin_status_stop()` writes the request once; `system_plugin_status_stop_poll()` reads without rewriting it. The app polls six times at 400 ms. An inconclusive probe is retried like a missing ack; only the attempt budget offers the confirmed force path. A refused 1337 still ends a stop when the shell is gone.
**Consequence:** A stop that takes up to 2.4 s succeeds without a force confirmation, and rewriting cannot restart the shell's teardown window. The force path remains available when the budget expires.
**Rejected:** blocking the UI thread for 1.5 s; writing the stop on every poll (restarts the teardown wait); treating the first inconclusive probe as force.
**Verification:** `[V]` 2,142 host checks pass; the shell-core suite covers the status format and state machine. `[C]` the real ack timing is a b141 hardware item.

## D95 — The FTP parser validates its inputs, and a cancelled REMOVE cancels its stop

**Status:** Accepted
**Context:** The re-review of b130 found that the forked `libftpvita` still formatted and parsed client input into fixed buffers without bounds: `PORT` ignored `sscanf`'s return and could overflow `ip_str[16]`, `TYPE` wrote an unbounded `%s` into `format_control[8]`, the command token used `%s` into `cmd[16]`, and `RNTO` read an uninitialised `rename_path`. The data-socket file descriptors were uninitialised on a fresh client. Separately, the app's REMOVE modal could be cancelled while the stop gate was already running; the gate then completed and removed the plugin the user had declined.
**Decision:** `libftpvita` checks the `PORT` conversion, rejects octets/ports above 255, and formats with `snprintf`; `TYPE` uses `%7s`; the command token uses `%15s` and the receive buffer is terminated; `RNTO` requires a prior `RNFR` and returns after a 550; every client is zeroed and starts with `data_sockfd`/`pasv_sockfd = -1`, and every close checks for a valid fd first. The app cancels a pending `STOP_REMOVE` when the REMOVE modal is dismissed or closed, and refuses a second remove while a stop is active.
**Consequence:** A malformed or hostile FTP command cannot write past a fixed buffer in SceShell, and a cancelled REMOVE cannot complete in the background. The `PORT` and `TYPE` checks also make the data connection fail cleanly instead of connecting to a garbage address.
**Rejected:** relying on newlib's `%s` behaviour; only checking the port after formatting; leaving `rename_path` to whatever the heap contained; letting the stop gate outlive its modal.
**Verification:** `[V]` the patched library builds on a clean pinned checkout, the shell links and audits at 70 imports, and 2,142 host checks pass; the parser subset has direct host tests. `[C]` the real client paths and the cancel/stop interaction are b141 hardware items.

## D96 — Directory removal is a whitelist; system directories are never touched

**Status:** Accepted (hard safety rule 8)
**Context:** COCKPIT writes into `ur0:tai`, `ur0:data`, and `ux0:data`, which are console-owned system locations. A generic `platform_remove_dir` plus a path constant is one typo away from deleting a directory the console needs to boot, and the b124/b141 work made the install/remove paths more active.
**Decision:** `platform_remove_dir` removes only a directory accepted by `core/path_guard` — in this release, `ur0:data/COCKPIT` with optional slash and case folding. Every volume root, `ur0:data`, `ux0:data`, `ur0:tai`, `ux0:tai`, `app0:`, the `ux0:data/COCKPIT` state directory, and any subdirectory of the COCKPIT leaf is refused with -1. `platform_make_dir` checks with `sceIoGetstat` first and never calls `sceIoMkdir` on an existing path; an existing file at the path is an error, not something to delete. The shell's `ensure_dir` does the same and fails control start on a non-directory. REMOVE deletes the known payload, status, control, temp, `plugin.log` files, then attempts the leaf rmdir as best effort; a non-empty directory (for example a log recreated by the still-resident shell) is left in place and does **not** set `REMOVE INCOMPLETE`.
**Consequence:** A constant or caller error can no longer delete `ur0:tai`, `ur0:data`, `ux0:tai`, or `ux0:data`. A completed payload removal is not reported as incomplete because a resident process recreated a log. The only directory COCKPIT can ever remove is its own runtime leaf, and only when it is already empty.
**Rejected:** a blacklist of names to protect (a new system path would be unprotected); deleting a non-empty directory recursively; treating a failed leaf rmdir as a payload failure; calling `sceIoMkdir` first and relying on `EEXIST` (that is what made the b124 install look at the wrong `st_mode` branch in the first place).
**Verification:** `[V]` `test_system_plugin` covers the whitelist (`ur0:data`, `ur0:tai`, `ux0:data`, `ux0:tai`, volume roots, and a leaf subdirectory are all refused) and REMOVE with an unrelated file left in the leaf (payload gone, file survives, plan succeeds); 2,142 host checks pass. `[C]` the real `sceIoRmdir`/`sceIoGetstat` calls are b141 hardware items.

## D97 — Close the review findings without widening the safety surface

**Status:** Accepted (remediation of the ce-code-review run on b139)
**Context:** The review confirmed a P0 in the FTP denial: `path_is_denied` normalized `..`, and a `..` that popped the volume produced `tai/boot_config.txt`, which missed both deny strings while the filesystem still resolved the real file. It also found that the hardened PORT parser asked for `%u` while the new `mini_sscanf` implemented only `%d` (every active-mode transfer returned 501), that the force paths could open POWER OFF after an unrecovered config write instead of DO NOT REBOOT, that a failed panel build latched the template flag for the boot, and that the removal whitelist, the stop resend, the cancel-REMOVE repaint, and several `[V]` claims had no test or had drifted.
**Decision:** `path_is_denied` rejects any path containing a `..` component outright and `path_is_protected_dir` refuses RMD and RNFR/RNTO for the volume roots, `ur0:data`, `ur0:tai`, `ux0:data`, `ux0:tai`, and the COCKPIT leaves. `mini_sscanf` implements `%u` (digits only, saturated at `UINT_MAX`, no negative sign). Force disable and force remove capture the commit result, set `plan.unrecovered`, keep the draft, and prefer DO NOT REBOOT over POWER OFF. The panel registers its liveness tick before the widget checks and clears the template latch when the box disappears, so a failed build retries on the next menu open. Ping retries keep the failed flag (BROKEN actions stay visible), the stop gate resends once mid-budget, and cancelling REMOVE refreshes the block. `path_guard` is narrowed to the `ur0:data/COCKPIT` leaf only; the fake applies the same whitelist; `ensure_dir` rejects a file. The shell's status file has one writer (the control thread publishes the first record).
**Consequence:** A traversal spelling cannot reach `boot_config.txt`, an FTP client cannot RMD a console directory, active-mode transfers work again, and an unrecovered write cannot be presented as a committed change. The host suite now covers the whitelist, the force-unrecovered plan, the write-once stop, and the exact `%u` PORT grammar; the docs no longer call the app-side stop gate host-tested.
**Rejected:** resolving `..` and comparing the result (the b139 bug); accepting `ux0:data/COCKPIT` in the removal whitelist; keeping the panel latch until module stop; clearing `s_ping_failed` on every retry; relying on the host fake's unconditional `mkdir` success.
**Verification:** `[V]` 2,142 host checks (133 metrics, 1,226 editor, 265 UI, 445 system-plugin, 73 shell-core), 47 previews, default and safe builds; b141 is 192,707 bytes, SHA-256 `fee86ca9da7b9f634a54a817e38338fbdb1607cef9a6d8edbb85e36ab5f4b4fc`; the import audit still resolves 70 imports and all three entry points. `[C]` the real FTP, panel lifetime, stop ack, and deletion paths remain hardware-unverified.

## D98 — Show the clock source, close the copy gap, and stop trusting PAF string labels

**Status:** Accepted (Batch A of the post-b14x review)
**Context:** R29 asks the self-plugin block to state whether clocks come from PSVshellPlus or `--`; R45 asks the install modal to state the clock source and that the plugin must be removed before uninstalling COCKPIT. Both were unaddressed. The review also found that the stop fallback accepted a single refused probe, that the impose root was cached for the module lifetime, and that the PAF component's string/memory stub labels were an unverified set after `strstr` turned out to be a tokenizer NID.
**Decision:** The block right-aligns `CLOCKS PSVSHELLPLUS` or `CLOCKS --` on the existing FTP line, so the fixed 72 px height is unchanged. The install modal copy now states both disclosures and fits the 512-byte body. The stop fallback requires two consecutive refused 1337 probes before it ends a stop without an ack. `cockpit_capture_impose_root` re-resolves the root every panel tick and drops cached widgets if the pointer changed. `paf_libc.c` implements `str*`/`mem*` locally; the remaining PAF imports are the heap, `vsnprintf`, and the PAF C++ string helpers that `paf.h` itself calls.
**Consequence:** The block meets R29 without a layout change, the install dialog meets R45, a single early probe cannot end a start, a recreated impose root cannot leave stale widget pointers, and only the PAF heap, `vsnprintf`, and PAF's own C++ string helpers remain to audit. A failed panel build still releases only when the menu closes (the template is already in the box); that is the deliberate duplicate-avoidance trade-off.
**Rejected:** adding a sixth block row (fixed-height contract); shortening the modal by dropping the FTP warning; accepting one refused probe; caching the root for the module lifetime; keeping the PAF `str*`/`mem*` imports because they "probably" match.
**Verification:** `[V]` 2,142 host checks, 47 previews, default and safe builds; the shell imports 70 symbols, the entry audit passes, and the block/modal render. `[C]` the clock text, modal wrapping on the device, PAF root lifetime, and the two-probe timing are b141 hardware items.

## D99 — Ship the dashboard and the plugin manager only; remove every plugin payload

**Status:** Accepted (supersedes D85 and the system-plugin scope of D86–D98)
**Context:** The b141 system plugin never reached `module_start`. A `*main` probe proved why: `sceSysmoduleIsLoaded(SCE_SYSMODULE_NET)` returned 0 inside SceShell, and the shell plugin's hard `SceNet`/`SceNetCtl` imports therefore kept the whole `*main` batch from starting, taking PSVshellPlus down with it. A diagnostic loader then called `sceSysmoduleLoadModule(SCE_SYSMODULE_NET)` and `sceKernelLoadStartModule("ur0:tai/cockpit_system.suprx")` from a `*main` plugin; the console stopped booting with `C1-14738-3`. The config was restored from `ur0:tai_bkp`, and the loader path is rejected. The two remaining designs (dynamic `taiGetModuleExportFunc` resolution in the shell plugin, or a separate worker plugin) both keep a resident SceShell process whose only user-facing feature is an FTP toggle. That is not worth a second boot-failure surface on a console with no recovery UI beyond hold-L and Safe Mode.
**Decision:** COCKPIT is one userland app. Remove `system-plugin/` entirely: the PAF shell, Quick Menu panel, RCO, manifest, FTP worker, `libftpvita` patch, PAF toolchain and import audit, the app-local clock bridge, and the diagnostic probe/loader. Remove the app-side install/update/repair/remove/force policy, self-plugin block, choice modals, private editor batches, path guard, payload/probe/status/control adapter calls, and every `SceNet`/`SceNetCtl` import that existed for them. The product is the HOME dashboard plus the PLUGINS transaction manager. CORE loses the ARM/GPU/BUS rows: without a kernel or bridge source the userland request is not the applied value, and the project does not print it. The VPK contains `eboot.bin` and `sce_sys/` only. The app writes only the active `config.txt` and its `.original`/`.session`/`.undo` snapshots; it never installs, deletes, or loads a module, and never opens `boot_config.txt`.
**Consequence:** There is no COCKPIT line to add, no boot hook, no resident shell process, and no network listener. A failed build or a bad toggle can only damage `config.txt`, which the manager already snapshots and the console can recover from hold-L. The cost is the Quick Menu FTP toggle and the applied-clock readout. The manager's transaction dock, restore points, battery-gated reboot, and inline write failures are unchanged.
**Rejected:** resolving `SceNet` dynamically after `sceSysmoduleLoadModule` (still a resident `*main` module and still unverified); moving the FTP worker to a second plugin loaded by a loader (the loader itself caused `C1-14738-3`); keeping the clock bridge because it "only" loads from `app0:` (an unverified module load with no manager value); leaving the payload files in the VPK but not installing them (dead weight and a misleading artifact).
**Verification:** `[V]` clean default and safe builds, 0 compiler warnings; `cockpit-v01.00-b143.vpk` is 129,118 bytes, SHA-256 `641b3b2bbcef1f73000565a3f77bddb836941fd65af8cc3396c15de5ffac3293`; the VPK contains only `eboot.bin` and `sce_sys/`; 1,293 host checks (113 metrics, 1,147 editor, 33 UI) and 38 previews pass; no COCKPIT source calls `SceNet`, `SceNetCtl`, or `sceKernelLoadStartModule`. `[V]` console evidence: the `*main` probe wrote `probe` and `net=0`; the loader produced `C1-14738-3`. `[C]` b143 has not run on hardware; the dashboard and manager checklist is the next action.

## D100 — Recovery preserves every copy and never offers a dead UNDO

**Status:** Accepted (2026-11-08 code-health review; supersedes the narrow `.old` handling of D71)
**Context:** `platform_write_config` deleted `<path>.old` before checking the live file. A crash between the two renames could leave `.old` as the only config, and a later write could delete it. Separately, an oversized live config advertised `.undo` but every commit refused with `CONFIG CHANGED ON DISK`, an unrecovered write left SAVE/CONFIRM visible while both refused, and a comment-only `.undo` was offered forever. The host fake could not produce `TOO_LARGE`, so none of these branches had a test.
**Decision:** Treat an absent live file with a surviving `.old` as a recovery case: rename `.old` back before any write, and otherwise fail closed with `RECOVERY`. Classify only a real `ENOENT` stat failure as absent; every other stat failure is unreadable. When no live config exists, probe `.original`, `.session`, and `.undo` on both mounts and let any readable snapshot set the restore source. For an UNDO commit, skip the stale-live check when the scan could not read or model the file (`too_large`/`unrecovered`), because the snapshot is the known-good source. When a snapshot write fails, probe the live `.undo`, recover an interrupted `.undo.old` swap, and only then rewrite the previous bytes; never report UNDO available when the file is gone. An unrecovered write is a repair state that offers UNDO, not a draft; SAVE/CONFIRM are gated by `editor_readonly_reason()`. A comment-only `.undo` is committable because no-change for UNDO compares bytes, not plugin rows. The host fake now has a file ceiling above the editor cap and a recovery-failure hook.
**Consequence:** The last config copy is not deleted, oversized/absent configs can be repaired from snapshots, and the dock no longer points at actions that always refuse. The new failure stages are `LIVE`, `RECOVERY`, and `APPEARED`.
**Rejected:** deleting `.old` unconditionally; classifying every stat failure as absent; reporting UNDO available from a byte comparison alone; showing SAVE/CONFIRM in the unrecovered state; adding a second write in the DATA-failure path when the old `.undo` is untouched.

## D101 — HOME reports the listed set, not a runtime-online claim

**Status:** Accepted (2026-11-08)
**Context:** `plugins_count()` includes present paths under `*PCSE00001` and `*NPXS*`, which are not loaded while HOME runs. The label `PLUGINS ONLINE` therefore overstated what the number meant, even though the group line separated the load targets.
**Decision:** Label the count `PLUGINS LISTED` (singular `PLUGIN LISTED`), and describe the block as the listed set. Keep the per-section counts. The per-row badge remains the authority on whether a specific entry is loaded.
**Consequence:** The headline no longer claims every title-scoped or system-app-scoped entry is running. The block still answers "how many entries are in the active config and present on disk?".

## D102 — Build identity includes the variant, packaged assets, and a build-directory-independent stamp

**Status:** Accepted (2026-11-08)
**Context:** The source hash covered `src`, `tools`, `CMakeLists.txt`, and `build.sh`, but not `--safe`, `sce_sys/`, or the VPK assets. A safe build could reuse a default build's number, `send.sh` could upload the wrong variant, and wiping `build/` lost the reuse stamp. The prune loop removed only `*.vpk`, leaving `.vpk.out`, `_param.sfo`, and pre-rename files behind.
**Decision:** Hash `src`, `sce_sys`, `CMakeLists.txt`, and `build.sh`, and append the `SAFE` variant to the hash. Keep `.source-stamp` beside `BUILD_NUMBER` in the project root. `build.sh` knows the exact expected VPK, deletes every other `cockpit-v*.vpk`, `.vpk.out`, and `.vpk_param.sfo`, removes the retired `system-plugin`/clock build outputs, and fails if the expected VPK is missing. `send.sh` has no fixed-name fallback and prints the variant and basename it uploads. CMake rejects `COCKPIT_BUILD=0`, so a manual build cannot silently produce `b0`.
**Consequence:** Default and safe builds get distinct numbers; an asset-only change gets a new number; wiping `build/` no longer burns a number for unchanged sources; the build directory has one VPK; the uploader states which variant it is sending.
**Rejected:** sharing a number between variants; hashing test/preview tools; keeping the stamp in `build/`; a fixed `build/cockpit.vpk` fallback; pruning only `*.vpk`.

### Verification for D100–D102 (b150)

`[V]` 1,371 host checks (130 metrics, 1,208 editor, 33 UI), 38 previews
(busiest 5,429/8,000), clean default and safe builds. b150 is 130,853 bytes,
SHA-256 `5143618f9affa68c275c1406819b32377c4662c3fccd143d0a9228a09f53555c`;
the VPK contains `eboot.bin` and `sce_sys/` only. New coverage: orphan `.old`
survival, oversized/comment-only/no-live undo, `.undo` rollback after a failed
snapshot, section moves and KERNEL-removal warnings, unrecovered repair state,
zero-frame history, per-field power validity, list-dir failure, fake
case-insensitivity, and the safe authid. `[C]` none of these paths has run on
the console; the hardware checklist is the next action.

## D103 — CORE reports system CPU load from the userland scheduler counters

**Status:** Accepted (2026-11-08; supersedes D6)
**Context:** D6 concluded that no userland API reports system-wide CPU utilisation and rejected idle-time sampling as measuring only this process. That premise was wrong. `sceKernelGetSystemInfo()` is a normal `SceLibKernel` export: `SceKernelSystemInfo` carries a cumulative idle clock, a wakeup count, and a thread-switch count for each of the four cores. PSVshell derives per-core load from the idle-clock delta over a microsecond tick; the same calculation works from userland without a plugin. CORE had lost its ARM/GPU/BUS rows in D99 and was left with memory only, which made the hero panel answer a narrower question than the machine it describes.
**Decision:** CORE shows four per-core CPU percentages, their active-core average, and context-switch/wakeup rates per second. The 20 s trace becomes aggregate CPU load on a fixed 0–100 % axis; the three memory pools stay as the second row. The platform adapter returns raw counters (`idle_us`, `time_us`, cumulative switches/wakeups, active mask); `core/metrics.c` owns the delta and percentage policy. A core outside `activeCpuMask` renders `--`, not 0 %. If an idle delta exceeds the elapsed time, the sample is refused and the panel renders `--`, so a finer-than-microsecond unit cannot become a fabricated percentage. A coarser unit makes the delta too small to detect from the counters; the hardware checklist treats a static 100 % at idle as unavailable. Each cumulative rate has its own validity flag: a total that is zero, unchanged, or lower than the previous sample renders `--` for that field, never a fabricated `0/S`. The applied ARM/GPU/BUS clocks remain omitted: the userland ScePower getters return this process's request, not the machine's value. GPU work is represented by the CDRAM/PHY pools and the pinned FRAME strip; there is no GPU-load or applied-GPU-clock API in userland.
**Consequence:** The hero panel answers "what is the machine doing, and how much memory is left?" with real system data. It adds no plugin, no boot surface, and no new import outside `SceLibKernel` (already linked). `sceKernelGetSystemInfo` and the microseconds interpretation of `idleClock` are `[C]` until a console run confirms them; a failed read, a finer-unit mismatch, or a stalled rate counter degrades to `--`, while a coarser unit is caught only by the static-100 % hardware check.
**Rejected:** keeping the memory-only panel; a kernel plugin or bridge for clocks; printing the ScePower request as `REQ`; deriving CPU load from this process's run time; a fabricated GPU load or `GPU CLK N/A` row; an autoscaling CPU trace that would make 5 % look like 95 %.
**Verification:** `[V]` host tests cover per-core percentages, the active mask, the aggregate, the rates, zero/frozen/regressed counters producing `--` per field, one rate advancing while the other is frozen, a delta larger than elapsed time failing closed, a failed read leaving the trace alone, and the trace data. `[V]` the preview renders the fixed 0–100 % axis and the CPU-absent and two-core `--` paths; no host test asserts the axis pixels. `[C]` `sceKernelGetSystemInfo` availability and `idleClock` units on 3.65 Enso are unconfirmed until the b156 console run.

## D104 — The launch card, the choice modal, and the ABOUT reboot share one safety model

**Status:** Accepted (2026-11-10)
**Context:** The plugin-manager-first layout made PLUGINS the first screen, so the app needed to say what it backs up before a user touches a row. The old modal layer had one error kind and treated Cross and Circle identically; an ABOUT confirmation built on it would have rebooted on Circle. Separately, the editor's `backup_warning` is set when a file is too large and stays clear when no config was read, and the `*_available` probes report a readable file, not this launch's write, so neither could tell the card the truth. The commit path already refuses to reboot after an unrecovered write, and ABOUT must not bypass that rule.
**Decision:** `core/plugin_editor` records `original_snapshot_present` (the original exists or was written at this scan) and `session_snapshot_written` (written from this scan's bytes). `ui/launch_card` composes the card from that result, the read status, and the editor's read-only reason; a failed snapshot, an unreadable or oversized config, and a missing config get separate bodies. `ui/modal` gains `UI_MODAL_SAFETY` (Cross or Circle acknowledges) and `UI_MODAL_REBOOT` (Cross confirms, Circle cancels), and draws one scrim under all kinds. `ui/about_text` owns the confirmation copy; `ui/plugin_flow` exposes `plugin_flow_reboot_block_reason()` as the single gate: invalid power, below 20 % while discharging, `write_unrecovered`, then `recover_failed`. ABOUT dims REBOOT and shows that reason without opening the confirmation; an open confirmation re-checks the gate every frame. The dock keeps its existing one-press actions.
**Consequence:** The first screen cannot claim a backup that failed, and a repair state cannot be bypassed from ABOUT. The card blocks the tab bar, the list, and the dock until acknowledged. The modal layer now has one choice surface, and the dock and ABOUT cannot disagree about why a reboot is blocked.
**Rejected:** inferring the card from `backup_warning` or the `*_available` probes; reusing the error modal for a confirmation; adding a confirmation step to the dock's existing actions; blocking a reboot for every read-only reason, including `NO CONFIG.TXT` and `CONFIG TOO LARGE`.
**Verification:** `[V]` host tests cover the four card bodies, the tab and ABOUT input routing, the four gate reasons, the four confirmation states, and the version/build mapping. `[V]` the preview renders the card (success, failed snapshot, no config, unreadable), ABOUT (allowed, blocked, unknown), and the confirmation (draft, preview over draft, preview clean, saved). `[C]` none of these screens has run on the console.

## D105 — PLUGINS is the landing screen; VITALS is second; ABOUT is third; the release is v1.01

**Status:** Accepted (2026-11-10)
**Context:** COCKPIT had been dashboard-first: HOME was the landing screen and the plugin manager sat behind it. The user's direction is the reverse. The manager is the reason to install the app; the machine readout is secondary. v1.00 was already published with b156, so a new build cannot reuse that tag.
**Decision:** `UiTab` is an explicit three-value enum: `UI_TAB_PLUGINS = 0`, `UI_TAB_VITALS`, `UI_TAB_ABOUT`. PLUGINS is the first tab and the launch screen; L and R step one tab and wrap. The dashboard tab and every current-state document are renamed VITALS. `VITA_VERSION` becomes `01.01`; the artifact is `cockpit-v01.01-b<build>.vpk`; the GitHub release tag is `v1.01`. ABOUT shows `1.01` and the build number from `tools/gen-build-id.sh`, which also feeds the host tests and the previews. The release is a normal release whose notes state the pending hardware checks; it is not a `verified` claim.
**Consequence:** A user lands in the manager, the machine readout is one press away, and the release keeps the v1 line without rewriting the published v1.00 tag. ABOUT, the VPK name, the host tests, and the previews share one version source, so they cannot drift.
**Rejected:** keeping VITALS as the landing tab; replacing the published v1.00 tag; jumping to v1.10 for a patch-sized change; a hardcoded version string in ABOUT.
**Verification:** `[V]` `VITA_VERSION` is `01.01`; the artifact is `cockpit-v01.01-b166.vpk`, 135,611 bytes, SHA-256 `70e3d9acc5bfefc6b2cdf238ce69df9b463b973c6501df1a2443aba2a5c5fe87`; the VPK contains `eboot.bin` and `sce_sys/` only; 1,491 host checks and 51 preview scenes pass; default and safe builds are clean. `[C]` b166 has not run on the console.
