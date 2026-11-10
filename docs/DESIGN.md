# Design

The visual system. Read this before changing anything that renders.

Reference: **Cyberpunk 2077's in-game HUD.** Not the Edgerunners anime. The game
HUD is precise and restrained — thin rules, one chamfered corner, small tracked
labels over large values. The anime is louder and bloomier, and its bloom does
not survive a 5-inch screen.

---

## 1. Screen

Fixed 960 × 544 output, no scaling (D13). The *panel area* is a scrolling
viewport onto a taller canvas (D45), so regions no longer have to fit on one
screen. The header is pinned. On VITALS the FRAME strip is pinned too; on
PLUGINS the ledger and its transaction dock own the bottom of the screen (D81).

```
┌──────────────────────────────────────────────────────────────┐
│ COCKPIT   PLUGINS  VITALS  ABOUT  L/R            14:22:07    │
├──────────────────────────────────────────────────────────────┤
│ CORE 552×210                 │ POWER 360×210                  │
│ C0 18%  C1 22%  C2 9%  C3 4% │                                │
│ USER 179  CDRAM 41  PHY   64 │             87%                │
│ CPU LOAD AVG 13%  CTX 842/S  │           CHARGING             │
│      ╱‾╲__╱‾╲__              │  TEMP  31   VOLT  3820         │
│                              │  CYC  214   SOH     98         │
├──────────────────────────────┼────────────────────────────────┤
│ STORAGE 552×172              │ SYSTEM 360×172                 │
│ UX0             12.4/29.5 GB │ FW                  3.65       │
│ ▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░  │ RUN           00:14:22         │
│ UMA0                   N/A   │ MODEL            PSVITA        │
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░  │                                │
├──────────────────────────────────────────────────────────────┤
│ PLUGINS 924×140   (on the canvas, below both columns)        │
│ 7  PLUGINS LISTED                                            │
│    KERNEL 3  SYSTEM 2  SYSTEM APPS 1                         │
├──────────────────────────────────────────────────────────────┤
│ FRAME  58.4 FPS  17.1 MS   ─┼─────┼───┼──                   │ 44 pinned
└──────────────────────────────────────────────────────────────┘
```

The PLUGINS tab replaces the panel area with a full-width ledger and a pinned
transaction dock; the header stays pinned and the FRAME strip is not drawn, so
the ledger runs to the bottom of the screen. ABOUT replaces it with one
satellite panel and no scrollbar. The drawing is schematic (the VITALS screen);
the table below is the exact geometry. The clock is the only identity
in the header. The device model lives in SYSTEM, so nothing appears twice. The
PLUGINS block is the last canvas panel: at scroll 0 it sits just below the fold,
and the scrollbar is the affordance that reaches it (D67).

### Grid

| Region | x | y | w | h |
|---|---|---|---|---|
| Header | 0 | 0 | 960 | 64 |
| CORE | 18 | 0 | 552 | 210 |
| POWER | 582 | 0 | 360 | 210 |
| STORAGE | 18 | 222 | 552 | 172 |
| SYSTEM | 582 | 222 | 360 | 172 |
| PLUGINS summary | 18 | 406 | 924 | 140 |
| FRAME | 18 | 482 | 924 | 44 | (pinned, screen coords, VITALS only) |
| ABOUT | 18 | 76 | 924 | 370 | (screen coords, no canvas, no scroll) |

Panel `y` is canvas coordinates; the app offsets them by the scroll and clips to
the band at y 64..472 on VITALS. The band carries a 12 px top inset, so the first
panel starts at screen y 76 and its outline is whole; the inset scrolls away. The
VITALS canvas is `546` (the PLUGINS lower edge), and `layout_max_scroll()` is 150
because `546 > 408 - 12`. The PLUGINS tab uses its own canvas and its own band:
`plugins_content_height()` starts at 0 and grows with the rows, groups, and
orphan section; its scroll range is `height - (PLUGINS_VIEW_H - VIEW_TOP_GAP - plugins_dock_height())`.
`PLUGINS_VIEW_H` is 472, so the ledger and dock reach screen y 536; the 8 px
bottom margin is the only inset.

Constants: `MARGIN 18`, `GAP 12`, `SP_3 12`, `VIEW_TOP_GAP 12`. Both columns
share both seams -- row one is 210 tall, row two 172 -- so the 12 px gap between
the rows runs unbroken across the screen and both columns end at canvas y 394.
The summary block starts 12 px below them and ends at 546. The strip begins at
screen y 482 and ends at 526, leaving an 18 px bottom margin.

### Vertical rhythm

Every vertical gap is one of six steps, declared in `src/gfx/theme.h`:

| Token | px | Role |
|---|---|---|
| `SP_1` | 4 | base unit; divides the 12 px column gap and the 18 px margin |
| `SP_2` | 8 | a readout to its meter or trace; a name to its description |
| `SP_3` | 12 | a hero number to its state; a panel to its neighbour |
| `SP_4` | 16 | one readout row to the next; the title rule to the first row |
| `SP_5` | 24 | last content to a panel's bottom edge |
| `SP_6` | 32 | reserved for a section break |

The first version used 2-7 px gaps against 21-35 px type, so a value, the row
below it and the caption under that read as one dense lump. Two rules now hold:
the gap between two blocks is always larger than the space inside a block, and
every gap is on the scale. A gap off the scale is a spacing bug the same way a
colour off the palette is a colour bug (D48).

Readout rows use `READOUT_STRIDE`, defined as `LINE_H(PX_VALUE) + SP_4` = 37.
Deriving the stride from the value height means changing the type ladder cannot
silently close the gap.

### Why it is not a grid of equal cards

Six identical panels in a 3 × 2 grid is the default dashboard, and it says
nothing about the machine. This layout has three tiers:

- **Hero** — CORE, 552 × 210, half again the area of any satellite. It carries
  the CPU load trend and the memory pools together, because headroom — CPU time
  and memory — is the question this panel answers.
- **Satellites** — STORAGE, POWER, SYSTEM. Sized by how much they hold.
- **Status strip** — FRAME, full width but only 44 px tall, because it measures
  the app rather than the machine and should not compete with the machine.

Every region is sized by information importance. If a region grows, something
else must shrink.

## 2. Palette

| Token | Hex | Role |
|---|---|---|
| `C_FIELD` | `#000000` | screen field |
| `C_PANEL` | `#0D0E12` | panel fill |
| `C_HAIR` | `#1C1D24` | scrollbar track, empty meter cells |
| `C_DIM` | `#6E707E` | labels, pipes, secondary actions, secondary text |
| `C_WHITE` | `#EDEDF0` | numeric values, off titles, the keycap triangle |
| `C_YELLOW` | `#FCEE0A` | primary — wordmark, hero accents, active tab, warning, `DRAFT`/`PREVIEW`, pending, exceptions, the primary dock action |
| `C_CYAN` | `#2AD8E8` | machine data, group headers, boot status, the config path |
| `C_GREEN` | `#3BE07A` | on/control — the rocker's on half, the `LOADED` badge, the ACTIVE/ENABLING counts and enabling names, and a `+` / `WILL ENABLE` diff marker |
| `C_GREEN_DIM` | `#2FA85C` | a plugin title active in the working set |
| `C_RED` | `#FF2E4D` | off state only — the DISABLED and DISABLING counts, the rocker's off half, the `NOT LOADED` badge |

`C_DIM` was `#43444F` until the host preview measured it at roughly 2:1 contrast
against `C_PANEL` — unreadable at 10 px on a 5-inch screen, and the clock
disappeared entirely when it fell back to `--`. The current value is about 4:1
(D20).

The field is true black, not a tinted near-black. On the OLED PCH-1000 this makes
panels float and saves power; on the LCD PCH-2000 it simply reads as dark.

`C_GREEN_DIM` and `C_RED` exist for the PLUGINS transaction (D78). Red is
deliberately narrow: the DISABLED count, the dock's DISABLING count, the
rocker's off half, and the `NOT LOADED` badge. It never colours a label, a
title, or an action, so it still cannot be mistaken for the hazard channel. `C_GREEN` remains the on/control
colour; `C_GREEN_DIM` is one step down so an active title reads as a state
instead of competing with the rocker. The rule that a metric number is never
coloured (D19) still holds; the PLUGINS counts are status tokens, not readouts
(D77, D78).

Accent assignment:

| Region | Accent |
|---|---|
| CORE | yellow |
| STORAGE | cyan |
| POWER | yellow |
| SYSTEM | cyan |
| FRAME | cyan at 60 % |

An accent appears in exactly four places: the panel title, the frame outline at
45 %, the hero's corner brackets, and that panel's own meters and traces. It
never appears in a metric readout — a metric number is always white, so
emphasis can never be mistaken for meaning (D19). The PLUGINS tab is not a
metric panel: its counts and titles are transaction tokens, governed by D77 and
D78 instead.

The earlier rule said "never in the data". That contradicted the accent table
above: if an accent only ever appeared in a title and an outline, the region
colour coding would be invisible. Meters and traces are the region's own
signal, so they carry it; numbers do not.

## 3. Typography

A hand-built 5 × 7 bitmap face (`src/gfx/font.c`). No font ships with VitaSDK
2026.08, and a TTF would add a licence and ~200 KB for no gain at these sizes.
Block glyphs also render as crisp rectangles under additive glow.

Sizes are *font-pixel scale*: `px=2` means one font pixel is a 2 × 2 screen rect.
A glyph is `5·px` wide, `7·px` tall, advancing `6·px`.

| Role | px | Box | Used for |
|---|---|---|---|
| micro | 1.5 | 7.5 × 10.5 | units, charging state, trace caption, plugin description, status badge, dock copy, header exception, group header, modal body |
| label | 2 | 10 × 14 | metric names, panel titles |
| value | 3 | 15 × 21 | numbers in a row |
| hero | 4 | 20 × 28 | wordmark |
| primary | 5 | 25 × 35 | the one big number in a panel |

Rules:

- Uppercase everywhere; lowercase is folded by the renderer.
- Labels are capped at 10 characters so the label column never reflows.
- Panel titles get one extra font-pixel of tracking (`font_text_tracked`).
- Two sizes share a baseline only in a readout, where the label is
  bottom-aligned to its value. Never top-align them.
- A secondary line (a plugin's description) is one step down the ladder from
  the line it explains, never the same size.
- UI labels are ASCII only. The 5 × 7 table has `&` and `,` because the dock
  needs them; it has no ellipsis, so truncation ends in `..`. A typographic
  dash, quote, or ellipsis in a label is a missing-glyph bug (KTD6).

## 4. Panel anatomy

**Satellite** — one chamfer, no brackets:

```
 ┌──────────────────────────────┐╲     chamfer 14 px, top-right
 │▏STORAGE                      │ ╲
 │──────────────────────────────│     hairline at 25 % accent
 │                              │
 │  UX0            12.4/29.5 GB │     label px=2 dim, value px=3 white
 │  ▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░  │     meter below, full content width
 │  UMA0                  N/A   │
 │  ░░░░░░░░░░░░░░░░░░░░░░░░░░  │
 └──────────────────────────────┘
```

**Hero** — larger chamfer, a second cut at the bottom-left, and brackets:

```
 ╱──────────────────────────────────────────────┐╲   28 px chamfers
 │▏CORE                                         │ ╲
 │──────────────────────────────────────────────│
 │  C0  18%   C1  22%   C2   9%   C3   4%       │
 │  USER   182     CDRAM   41     PHY     64    │
 │  CPU LOAD AVG 13%   CTX 842/S  WAKE 230/S    │
 │  ▁▂▃▄▅▆▇█▇▆▅▄▃▂▁▂▃▄▅▆▇▆▅▄▃▂▁                │
 ╲──────────────────────────────────────────────┘
  ↑ brackets, 16 px, full accent, on the two square corners
```

The extra cut and the brackets are the only things that distinguish the hero from
a satellite. Hierarchy is carried by size and corner treatment, not by colour or
decoration.

CORE has no clock row. The applied clocks required a bridge or kernel reader,
and both were removed with the system plugin (D99). The userland request is not
the applied value, so the row would have been permanently `--`. The CPU rows and
the load trace use the space instead (D103). CORE also has no GPU-load or
applied-GPU-clock row: userland exposes neither. The CDRAM/PHY pools and the
pinned FRAME strip are the GPU view.

## 5. Readouts

The core pattern:

```
LABEL                     VALUE
px=2 dim                  px=3 white, right-aligned
```

Inside a multi-column block (CORE), a column is `theme_readout(label, value,
valid, x, y, width)`: label at `x`, value right-aligned to `x + width`. One
function serves both a full-width row and a column, because they differ only in
the width.

A panel's primary number may escalate to `px=5`. Only one per panel, and it
stays `C_WHITE`: scale carries the emphasis, not colour (D19). In CORE the
escalated element is the trace, not a number.

SYSTEM is the exception that proves the rule: its firmware row stays at `px=3`
because the version string is unbounded, and at `px=5` a ten-character version
overprints its own caption (D27). A primary number is only safe where the value
has a known width.

Meters are segmented, ten cells, filled at full accent and empty at `C_HAIR`.
Segments communicate discrete capacity; a continuous bar would imply precision
the data does not have. A meter with no reading draws its empty track at 70 % of
`C_HAIR`: at 45 % it rendered darker than the panel fill and vanished, which is
the opposite of what an absent track is for.

In STORAGE the meter sits *below* its readout and spans the panel's content
width, so a volume reads as one block: name, size, capacity. The first version
put the meter inline at x=320, which stranded the value mid-panel and forced the
two rows into a 34 px stride -- too short for the row-two grid (D48).

STORAGE is the one place `--` is not used. A mount that is not present shows
`N/A`, and a refused read shows `ERR` plus its errno byte. "No volume in this
slot" and "this metric could not be read" are different statements, and an empty
row is never acceptable (D41).

Traces are a 2 px line, not a filled area. At 200 samples across ~500 px an
area chart collapses into a solid block that hides the shape it exists to show
(D21). A trace is always scaled to an axis that is printed beside it, so an
autoscaled trace declares its range instead of implying one.

### Tabs

The 64 px header carries the tabs between the wordmark and the clock. `PLUGINS`,
`VITALS`, and `ABOUT` are label-scale, 1 px tracked, at x 210, 310, and 400; the
active tab is `C_YELLOW` with a 2 px underline at y 36, the inactive tab is
`C_DIM`, and `L/R` sits at x 480. The header carries no plugin count: that is a
panel on VITALS (D67). The hazard band sits at y 48, the line below the tabs, so a
fault and the tabs coexist.

### PLUGINS summary block

The block is a satellite panel at x 18, w 924, below both columns at canvas y
406, 140 px tall. It is one column of information, read left to right and top to
bottom:

- a primary-scale count (`7`) at the content inset, tinted `C_GREEN` when the
  count is non-zero and `C_WHITE` at zero; it counts every path the active
  config lists whose file exists, including title-scoped entries;
- `PLUGINS LISTED` at label scale beside it, aligned to the count's bottom
  (singular `PLUGIN LISTED` for one);
- one micro `C_DIM` line of non-empty group counts in load order: `KERNEL 3
  SYSTEM 2  SYSTEM APPS 1  ALL TITLES 1  TITLES 0`. A config with no activated
  plugins says `NO PLUGINS LISTED` instead of an empty line.

`CONFIG WARNING` sits right-aligned on the group line in `C_YELLOW` when a
stray path or a truncated row was seen. The count is the listed set, not a
per-title runtime claim; the group line separates KERNEL, SYSTEM, SYSTEM APPS,
ALL TITLES, and TITLES. A pending edit is state belonging to the PLUGINS tab
that made it, not a VITALS reading (D68).

A config problem replaces the count with `--` and names the state beside it:
`NO CONFIG.TXT` with `UR0:TAI OR UX0:TAI`, `CONFIG UNREADABLE` or
`CONFIG TOO LARGE` with `READ ONLY`, all in `C_YELLOW`.

The block is read-only; every action lives on the PLUGINS tab. It is fixed in
height because its content is a summary, and it sits below the fold at scroll 0,
so VITALS carries a scrollbar whose thumb reaches it. The block has no ceiling to
hit: the group line is one line regardless of plugin count.

### PLUGINS tab

The tab is a full-width panel at x 18, w 924, starting at canvas y 0. Its height
is computed, not compiled in. `plugins_content_height()` counts the three header
slots, the group headers, the 52 px rows, the orphan section, and the preview's
`REMOVING` group; `plugins_max_scroll()` subtracts the pinned dock, so no row is
ever hidden behind it. The band clips the canvas and the scrollbar follows.

The panel draws three truths side by side: the working set (title and split
rocker), the launch set (the status badge), and the next-boot set (`NEXT ON` /
`NEXT OFF`). A staged or saved change never repaints the launch badge (D69,
D77).

#### Header

The header has exactly three 14 px line slots and no fourth line (KD11). The
height never changes, so an exception can never shift the ledger below it.

1. **Source.** `USING` in `C_DIM`, the active path in `C_CYAN`, then the state:
   `DRAFT` or `PREVIEW` in `C_YELLOW`, a dim pipe, the ACTIVE count in `C_GREEN`
   (dim at zero) with a dim `ACTIVE` label, a dim pipe, the DISABLED count in
   `C_RED` (dim at zero) with a dim `DISABLED` label, and an optional yellow
   `N NEXT BOOT` segment. ACTIVE counts working-set rows with at least one
   reached, uncommented line; DISABLED counts every other row. NEXT BOOT counts
   working-set rows whose activity differs from the launch set and appears only
   when nonzero.
2. **Exception.** Blank when there is nothing to say. Otherwise a pipe-separated
   line in `C_YELLOW`, in precedence order: read-only reason, config warning,
   backup warning, shadowed path, recovered. Segments append only while they fit
   the panel inset; the last one is cut with `..`.
3. **Prompt.** `PRESS` in `C_DIM`, a 14 × 12 chamfered keycap with a white
   outline triangle, then `FOR DOCK` in `C_DIM`. Triangle focuses the
   dock.

The old `ROWS / LOADED / NEXT BOOT` line, `CROSS CHANGE`, and the undo-status
text are gone (R14).

#### Rows

Rows are grouped by the most privileged line that names the path: KERNEL,
SYSTEM (`*main`), SYSTEM APPS (`*NPXS*`), ALL TITLES (`*ALL`), and TITLES. Each
non-empty group opens with a micro, 1 px-tracked cyan header. A path listed under
several targets is one row (D51, D56).

A row is a two-line entry. The left status dot is gone; the name starts at the
panel pad (`PANEL_PAD`, 14 px) on the first line and the description on the
second. The name is the basename; when two rows share a basename the full path is
shown. A description that does not fit is truncated with `..` at the badge.

The title is `C_GREEN_DIM` when the working set has at least one reached active
line, otherwise `C_WHITE`. The right control is a 46 × 20 split rocker with no
text: the left half is `C_RED` for off, the right half is `C_GREEN` for on, a
mixed row lights both, and an unswitchable row draws the same shape at 45 %. A
fully enabled row therefore lights its right half; a fully disabled row lights
its left half. The rocker describes the working set; it never claims the launch
set.

The status badges describe the launch set, not the file being edited:
`LOADED` (`C_GREEN`), `NOT LOADED` (`C_RED`), `MIXED` (`C_YELLOW`), `MISSING`
(`C_YELLOW`), `NOT REACHED` (`C_DIM`), and `BOOT` (`C_CYAN`). A locked HENKAKU
row adds a dim `LOCKED` badge beside its status. A row whose working set differs
from launch adds a yellow `NEXT ON` or `NEXT OFF` badge. Badges are 12 px high,
micro scale, tinted at 16 %, and stack leftwards with 6 px gaps. The badge row
sits 4 px below the rocker and shares the description baseline (R18-R20).

The selected row uses a 4 % white wash, the existing 2 px left bar, and a 1 px
white rocker outline. A draft-modified rocker adds a 6 px yellow corner notch
(R21).

Cross stages the row the editor says is actionable. A non-actionable row does
nothing: the status badge or the header exception names why, and no dialog opens
(R22). A missing active file can be disabled; a missing inactive file cannot be
enabled; a `MIXED` row disables first (D66, D69).

The orphan section follows the last group under the header
`INSTALLED - NOT IN CONFIG`. Its caption is `NOT LISTED IN THE ACTIVE CONFIG`
when the listing is complete; a failed directory read replaces it with
`EXCLUSIONS UNVERIFIED`. `boot_config.txt` is not consulted, so the section never
claims a file is not loaded by Enso. Files in a shadowed directory are prefixed
`SHADOWED`. The list is sorted, capped at 32, and states `MORE FILES NOT SHOWN`
when the cap is reached. Orphans are read-only (D61).

#### Restore preview

Selecting `RESTORE ORIGINAL`, `RESTORE PREVIOUS SESSION`, or `UNDO LAST WRITE`
replaces the list with a read-only plugin-level diff against the current working
set (the draft when one exists). The dock names the snapshot (`PREVIEW -
ORIGINAL`, `PREVIEW - PREVIOUS SESSION`, `PREVIEW - UNDO`) and counts the
proposed switches.

A changed row keeps its name; a `+` in `C_GREEN` marks a row that will be
enabled, a `-` in `C_YELLOW` marks one that will be disabled, and the right side
says `WILL ENABLE` or `WILL DISABLE`. Unchanged rows are dimmed. Paths the
snapshot would drop are listed under a cyan `REMOVING` header with `-` and
`WILL REMOVE`. The preview is read-only: Cross on a preview row does nothing
(R41, R44).

If a draft exists the dock warns `DRAFT WILL BE DISCARDED` and the confirm chip
reads `DISCARD DRAFT & RESTORE`. A snapshot that would remove or disable a BOOT
module or a locked HENKAKU path is refused; the path is named in
`RESTORE DROPS <path> - NOT ALLOWED`. The check covers a path behind a `*!` halt
as well as a fully commented one. A snapshot whose plugin set matches the
working set says `NO CHANGES - NOTHING TO RESTORE`. In both cases confirm and
save are unavailable and nothing is written (R42, R56).

A live file the parser could not read or model is not a restore target: the
pre-restore `.undo` would be empty or truncated, and the bytes being replaced
were never seen. The restore chips are dim and the dock names `CONFIG
UNREADABLE`, `CONFIG TOO LARGE`, `TOO MANY LINES`, or `TOO MANY PLUGIN ROWS`.
`UNDO LAST WRITE` remains available because it repairs an absent, unreadable, or
partially replaced file (D79).

#### Transaction dock

A permanent inline dock is pinned to the bottom of the PLUGINS panel. It is part
of the panel body, not a dialog: the list viewport shrinks by the dock height and
the dock never covers a row. It has a 1 px hairline on the top, side, and bottom
edges, and a 2 px yellow left edge while a draft or preview is active (R23, R32).

| State | Height | Title / summary | Actions |
|---|---|---|---|
| idle | 64 | none; the restore buttons are the whole strip | `RESTORE ORIGINAL`, `RESTORE PREVIOUS SESSION` |
| draft | 159 | `DRAFT - NOT WRITTEN`; `ENABLING n` (green count and names), `DISABLING n` (red count, white names), `+N MORE` | `DISCARD`, `SAVE, REBOOT LATER`, `CONFIRM & REBOOT`; second row: the two restores |
| preview | 123 | `PREVIEW - <snapshot>`; `ENABLING n`, `DISABLING n`, `REMOVING n` | `CANCEL`, `SAVE, REBOOT LATER`, `CONFIRM & REBOOT` |
| saved | 111 | `SAVED - REBOOT TO APPLY` | `UNDO LAST WRITE` and `REBOOT NOW` when an `.undo` differs; `REBOOT NOW` only otherwise |
| last write | 111 | `LAST WRITE - UNDO AVAILABLE` | `UNDO LAST WRITE` |

The state block uses 12 px of inner padding above the title and the warning line
leaves 12 px below it, so the dock reads with the same air at the top and bottom.
The idle strip is already balanced at 15 px above and below its 34 px buttons.

The safe action is focused first (`RESTORE ORIGINAL`, `DISCARD`, `CANCEL`,
`UNDO`, or `REBOOT NOW`). In a draft or preview the primary action is the
rightmost available commit: `CONFIRM & REBOOT` normally, `SAVE, REBOOT LATER`
when the battery blocks a reboot. In saved/last-write the primary is `REBOOT NOW`
or `UNDO LAST WRITE`. Unavailable actions stay in place, dimmed, and Left/Right
skips them, so the row never reflows (R28).

Action buttons are slabs, not text chips. The primary is a solid yellow fill with
black text, a 1 px light top edge, and a 3 px darker yellow bottom ledge. A
secondary action is a dark fill with a 1 px top highlight, 1 px side borders, and
a 2 px black bottom ledge. The selected action gets a 2 px white outline offset
4 px from the slab, so focus reads without changing the fill. An unavailable
action keeps the slab shape but loses the bevel and the bright text. Labels use
the label scale (px 2) and are centred; the title uses the same scale, the
summary and warning use micro (px 1.5). A 1 px hairline separates the state block
from the action area. Warnings sit on their own bottom line behind a 2 px yellow
marker; an idle restore failure draws its reason beside the restore buttons
behind the same marker.

#### Modal layer

Three modal kinds share one scrim and the panel chrome:

- `UI_MODAL_ERROR` — the unrecoverable-write `DO NOT REBOOT` panel. 600 × 240,
  up to ten micro lines, Cross or Circle closes it. Every other failure is
  inline on the dock, because it is retryable and the draft or preview stays
  behind it (R38, D99).
- `UI_MODAL_SAFETY` — the launch gate. 720 × 260 at y 130, a label-scale body
  of up to eight lines, Cross or Circle continues. It is the first screen and
  blocks the tab bar, the list, and the dock.
- `UI_MODAL_REBOOT` — the ABOUT choice. 600 × 240, a label-scale body of up to
  six lines, `CROSS CONFIRM   CIRCLE CANCEL`. Cross sends the request; Circle
  closes without one.

A single `theme_scrim` pass sits under all three, so the app behind is
recognizable but reads as unavailable. The safety card's body is composed from
the launch result; a failed snapshot, an unreadable or oversized config, and a
missing config each get their own sentence.

Dock copy is exact and ASCII:

- `SAVE ONLY - NOTHING CHANGES UNTIL THE VITA RESTARTS / THE PLUGINS RUNNING NOW STAY LOADED.`
- `REBOOT BLOCKED - CHARGE TO 20%` or `BATTERY LEVEL UNKNOWN - CHARGE BEFORE REBOOTING`
- Errors are inline: `WRITE FAILED <stage>`,
  `CONFIG CHANGED ON DISK. RESTART COCKPIT.`, `RESTART FAILED. TURN OFF AND ON.`
  A failed restore from idle has no state change, so its reason is drawn in the
  idle strip beside the dimmed chips (for example `NO RESTORE POINT` or
  `CONFIG TOO LARGE`).
- A failed write keeps the draft or preview and the file unchanged or recovered.
  An unrecoverable write is the one interrupt: the `DO NOT REBOOT` panel names
  the risk (R38).
- The kernel warning names the `HOLD L` escape hatch; a storage or licence
  warning from the batch takes precedence over the generic copy (R25, D58).

`DISCARD` clears the draft, writes nothing, and returns to the saved state when a
save is pending, otherwise idle. `SAVE, REBOOT LATER` writes the batch and leaves
the saved state; the running plugins stay loaded until restart.
`CONFIRM & REBOOT` writes the batch and requests a cold reset. `UNDO LAST WRITE`
opens the `.undo` preview under the same confirm/cancel rules as a restore. After
a relaunch the app cannot prove the saved config booted, so the dock shows
`LAST WRITE - UNDO AVAILABLE`, never a false `SAVED` banner (R27, R33-R39, D75).

#### Input

The sticks scroll continuously; the D-pad chooses (D76). The larger vertical
stick deflection wins, with a 0.18 dead zone, a squared response after the dead
zone, and 520 px/s at the rim. The result is clamped and applied to the active
surface: the VITALS canvas, the PLUGINS list, or the restore preview. VITALS
keeps a stepped D-pad scroll because it has no selection; on PLUGINS the stick is
the only way past the last row into the orphan section.

On PLUGINS the D-pad moves the row selection. Down from the last row enters the
dock; in the read-only preview, Down enters it from anywhere. Up returns to the
list, Left/Right selects an available dock action, Cross stages a row or
activates the focused dock action, Circle returns from the dock, and Triangle
toggles dock focus. L/R step tabs and wrap. The preview list is read-only and
Cross on it does nothing. The modal layer takes Cross or Circle; the reboot
confirmation is the only one with two outcomes, and the safety card blocks the
tab bar, the list, and the dock until it is acknowledged (R29, R45-R48).

#### Commit, restore, and snapshots

A switch press changes only `EditorState.text`; `last_text` is the file baseline
(KTD1). `editor_stage_toggle` rebuilds the working text from itself and reparses
it, with no read, stat, or write. The draft is memory-only: leaving COCKPIT
discards it and no draft file exists. One batch is one undo unit.

A commit re-reads the live file, refuses if it differs from the scan baseline
(`CONFIG CHANGED ON DISK. RESTART COCKPIT.`), writes `.undo` from the pre-batch
bytes, writes the working text once through the verified transaction, and adopts
it as the new baseline (D72). A save that succeeds leaves `pending_count` alone:
the rows still say `NEXT ON` / `NEXT OFF` because the launch set has not changed.
A restore reads the snapshot, previews it, and commits by writing the whole
snapshot; when a live file exists it snapshots the pre-restore file to `.undo`
first. An UNDO preview writes the `.undo` bytes it already read and never
replaces `.undo` from the live file; the snapshot is removed only after a
verified undo. With no live config, the undo path writes the `.undo` bytes
without a new snapshot (KTD5, D74).

The active config has two stable restore points plus the contextual undo:

- `config.txt.original` is written once and never overwritten.
- `config.txt.session` is the session-start file and is rewritten each session.
- `config.txt.undo` is the one-step pre-write copy; a commit replaces it, and a
  completed undo removes it.

No `.bak`, `.1.bak`, or `.2.bak` is created. The adapter writes a verified
`.new`, moves the original to `.old`, moves the sibling into place, and reads the
live file back; a failure restores the original and a failure to restore is
reported as `RESTORE`. At the next scan, an absent live file with a surviving
`.old` is moved back before the config is declared missing. The raw transaction
files are excluded from the orphan list (D61, D64, D71). `boot_config.txt` is
never opened, read, copied, or written.

## 6. Motion

| Motion | Trigger | Purpose |
|---|---|---|
| Trace scroll | sample rate | carries 20 s of history |
| Interference sweep | every ~5 s | proves the render loop is running |
| Wordmark chroma | static | the one loud element |

Nothing else animates. No value pulses, no hover states, no easing curves.

## 7. Restraint

The screen has exactly one loud element: the wordmark. Everything else is a thin
rule, a small label, or a number.

Two devices that would otherwise be decoration are load-bearing instead:

- **The hazard band is an alert channel.** It sits at y 48, on the line below the
  tabs, and renders only when something is wrong — battery under 20 % while
  discharging, a failed memory sample, or `ux0:` not answering. It never reflows
  the layout. On a healthy console it is absent and the screen is quiet.
- **The hazard band is the only alert, and it is yellow.** There is no second
  alert colour. The band is distinguished by its pattern — hard diagonal stripes
  — not by a hue that appears nowhere else, so it cannot be confused with
  decoration and nothing else can be mistaken for it. `C_RED` is an off-state
  token, not an alert: it is scoped to the four PLUGINS surfaces named in D78.

## 8. What not to do

- Do not return to a uniform grid. The asymmetry is the design.
- Do not introduce a second alert colour. The band is the alert.
- Do not put brackets on a satellite. They mark the hero.
- Do not add a third chamfer size.
- Do not glow anything except the wordmark and the hazard band.
- Do not introduce a size between the five above.
- Do not animate a value that did not change. The dock has no animation.
- Do not draw a metric the platform cannot report. `--` is correct; a guess is
  not.
- Do not add an icon, glyph, or shape that encodes nothing.
- Do not fill a trace as an area. A 2 px line, scaled to a printed axis.
- Do not colour a metric number. Emphasis in a readout is size, never hue. The
  PLUGINS status counts are tokens, not readouts (D77, D78).
- Do not use `C_GREEN` for a decorative highlight. It means "on" or "possible
  action", never "look here". `C_GREEN_DIM` is only an active title.
- Do not use `C_RED` for a label, title, action, or alert. It is the DISABLED
  and DISABLING counts, the rocker's off half, and the `NOT LOADED` badge.
- Do not open a dialog for a row action. The dock is the transaction surface;
  only an unrecoverable write interrupts.
- Do not add a fourth header line or let an exception move the ledger.
- Do not put a non-ASCII glyph in a UI label. Use `..` for truncation and the
  `&`/`,` the font already carries.

## 9. Preview references

The host preview writes these states under `build/preview/`; the copies in
`docs/` are the renders this document describes. Every value in them is
synthetic.

| State | Image |
|---|---|
| PLUGINS landing | [preview-plugins.png](preview-plugins.png) |
| PLUGINS header | [preview-header-normal.png](preview-header-normal.png) |
| Row states and badges | [preview-rows.png](preview-rows.png) |
| NEXT ON / NEXT OFF | [preview-plugins-next.png](preview-plugins-next.png) |
| Restore preview | [preview-preview-list.png](preview-preview-list.png) |
| Draft dock | [preview-dock-draft.png](preview-dock-draft.png) |
| Saved dock | [preview-dock-saved.png](preview-dock-saved.png) |
| VITALS: CORE, CPU and memory | [preview-healthy.png](preview-healthy.png) |
| VITALS: plugin summary | [preview-vitals-plugins.png](preview-vitals-plugins.png) |
| VITALS: config warning | [preview-vitals-plugins-warning.png](preview-vitals-plugins-warning.png) |
| Launch safety card | [preview-safety-card.png](preview-safety-card.png) |
| Launch card: failed snapshot | [preview-safety-card-failed.png](preview-safety-card-failed.png) |
| ABOUT | [preview-about.png](preview-about.png) |
| ABOUT: reboot blocked | [preview-about-blocked.png](preview-about-blocked.png) |
| ABOUT: reboot confirmation | [preview-reboot-draft.png](preview-reboot-draft.png) |

The VITALS images ([preview-healthy.png](preview-healthy.png),
[preview-vitals-plugins.png](preview-vitals-plugins.png), and
[preview-vitals-plugins-warning.png](preview-vitals-plugins-warning.png)) are
referenced by the project `README.md`.

## 10. Launcher assets

The bubble icon is `sce_sys/icon0.png`, 128 × 128. It is a black field with the
same chamfered yellow frame the panels use, a heavy yellow `C`, and a cyan
scanline with an end block. It is text-free so it reads at launcher size; the
system draws the app name beside it.

The LiveArea uses the same mark. `sce_sys/livearea/contents/bg.png` is 840 × 500:
a black HUD field with the chamfered frame, a low-opacity `C` watermark on the
right, and the yellow/cyan rule at the bottom. `startup.png` is 280 × 158 and
pairs a smaller mark with the `COCKPIT` wordmark and `TAIHEN CONFIG MANAGER`.

The master art is vector; the shipped files are the 128 px icon, the 840 × 500
background, and the 280 × 158 startup image. The icon is repeated in
`screenshots/09-icon.png` for the announcement.
