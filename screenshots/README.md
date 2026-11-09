# Screenshots

Images for the community announcement. All eight are 960 × 544, the app's native
output size.

They are **host preview renders**, not console captures: `tools/preview/run.sh`
draws the shipped composition through the software vita2d shim against the fake
platform. The layout, palette, type, and panel states are the ones the VPK
ships; the readings in them are fixtures.

| File | Scene | What it shows |
|---|---|---|
| `01-plugins.png` | `preview-plugins` | PLUGINS, the landing screen: ledger grouped by load target, launch and next-boot badges, dock |
| `02-vitals.png` | `preview-healthy` | VITALS: CORE with four CPU loads and the trace, POWER, STORAGE, SYSTEM, FRAME |
| `03-plugin-draft.png` | `preview-dock-draft` | A staged batch: `DRAFT - NOT WRITTEN`, the three dock actions, the kernel warning |
| `04-plugin-restore.png` | `preview-dock-preview` | A restore preview with `WILL ENABLE` / `WILL DISABLE` / `WILL REMOVE` |
| `05-launch-card.png` | `preview-safety-card` | The launch safety card: what COCKPIT manages and what this session backed up |
| `06-about.png` | `preview-about` | ABOUT: product, version, build, guarded REBOOT, and the real power-off path |
| `07-reboot-confirm.png` | `preview-reboot-draft` | The ABOUT confirmation when a staged batch would be discarded |
| `08-core-unavailable.png` | `preview-cpu-absent` | A failed CPU read: every CPU field renders `--`, never zero |

## Regenerate

```sh
./tools/preview/run.sh          # writes build/preview/*.png
```

The copies here are a curated set. `build/preview/` holds every scene,
including the empty, read-only, write-error, and stress states that are not
useful for an announcement.

## Posting

Reddit strips local paths from a self-post, so upload the images to the post
gallery rather than linking this directory. `01-plugins.png` is the hero;
`03`, `04`, `05`, `06`, and `07` are the ones that explain the manager and the
new launch gate.
