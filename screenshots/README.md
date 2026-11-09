# Screenshots

Images for the community announcement. All eight are 960 × 544, the app's native
output size.

They are **host preview renders**, not console captures: `tools/preview/run.sh`
draws the shipped composition through the software vita2d shim against the fake
platform. The layout, palette, type, and panel states are the ones the VPK
ships; the readings in them are fixtures.

| File | Scene | What it shows |
|---|---|---|
| `01-home.png` | `preview-healthy` | HOME: CORE with four CPU loads and the trace, POWER, STORAGE, SYSTEM, FRAME |
| `02-home-plugins.png` | `preview-home-plugins` | HOME scrolled to the plugin summary block |
| `03-plugin-ledger.png` | `preview-plugins` | The ledger, grouped by load target, with launch-state badges |
| `04-plugin-draft.png` | `preview-dock-draft` | A staged batch: `DRAFT - NOT WRITTEN`, the three dock actions, the kernel warning |
| `05-plugin-restore-preview.png` | `preview-dock-preview` | A restore preview with `WILL ENABLE` / `WILL DISABLE` / `WILL REMOVE` |
| `06-config-warning.png` | `preview-warning` | A config that names a missing file, a not-loaded plugin, and a config warning |
| `07-core-unavailable.png` | `preview-cpu-absent` | A failed CPU read: every CPU field renders `--`, never zero |
| `08-reboot-blocked.png` | `preview-dock-battery-blocked` | The battery gate: `REBOOT BLOCKED - CHARGE TO 20%`, save stays available |

## Regenerate

```sh
./tools/preview/run.sh          # writes build/preview/*.png
```

The copies here are a curated set. `build/preview/` holds all 40 scenes,
including the empty, read-only, write-error, and stress states that are not
useful for an announcement.

## Posting

Reddit strips local paths from a self-post, so upload the images to the post
gallery rather than linking this directory. `01-home.png` is the hero;
`03`, `04`, and `05` are the ones that explain the plugin manager.
