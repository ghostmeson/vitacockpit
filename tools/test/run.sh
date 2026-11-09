#!/usr/bin/env bash
set -euo pipefail

# Host tests for the sampling policy. Links the real core against the shared
# fake platform. Not part of the app build.
#
#   tools/test/run.sh

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/build/test"

mkdir -p "$OUT"
mkdir -p "$ROOT/build/generated"
"$ROOT/tools/gen-build-id.sh" "$ROOT/build/generated/cockpit_build.h" OFF >/dev/null

# The safe variant is a distinct identity; prove the generator records it.
SAFE_HDR="$OUT/cockpit_build_safe.h"
"$ROOT/tools/gen-build-id.sh" "$SAFE_HDR" --safe >/dev/null
grep -q '#define COCKPIT_SAFE 1' "$SAFE_HDR"
rm -f "$SAFE_HDR"

CC="${CC:-cc}"

"$CC" -std=c99 -O1 -g \
  -Wall -Wextra \
  -I"$ROOT/src" \
  -I"$ROOT/build/generated" \
  -I"$ROOT/tools/fake" \
  "$ROOT/tools/test/test_metrics.c" \
  "$ROOT/tools/fake/fake_platform.c" \
  "$ROOT/src/core/history.c" \
  "$ROOT/src/core/metrics.c" \
  "$ROOT/src/core/plugins.c" \
  -o "$OUT/test_metrics"

"$OUT/test_metrics"

"$CC" -std=c99 -O1 -g \
  -Wall -Wextra \
  -I"$ROOT/src" \
  -I"$ROOT/build/generated" \
  -I"$ROOT/tools/fake" \
  "$ROOT/tools/test/test_editor.c" \
  "$ROOT/tools/fake/fake_platform.c" \
  "$ROOT/src/core/input.c" \
  "$ROOT/src/core/history.c" \
  "$ROOT/src/core/metrics.c" \
  "$ROOT/src/core/plugin_editor.c" \
  "$ROOT/src/core/plugins.c" \
  "$ROOT/src/ui/launch_card.c" \
  "$ROOT/src/ui/plugin_flow.c" \
  "$ROOT/src/ui/plugin_nav.c" \
  -o "$OUT/test_editor"

"$OUT/test_editor"

"$CC" -std=c99 -O1 -g \
  -Wall -Wextra \
  -I"$ROOT/src" \
  -I"$ROOT/build/generated" \
  "$ROOT/tools/test/test_ui.c" \
  "$ROOT/src/ui/core_text.c" \
  "$ROOT/src/ui/plugin_nav.c" \
  -o "$OUT/test_ui"

"$OUT/test_ui"
