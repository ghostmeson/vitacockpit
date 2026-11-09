#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -z "${VITASDK:-}" ]]; then
  if [[ -d "$ROOT/../vitasdk" ]]; then
    VITASDK="$ROOT/../vitasdk"
  else
    VITASDK="$HOME/vitasdk"
  fi
fi
export VITASDK
export PATH="$VITASDK/bin:$PATH"

if [[ ! -d "$VITASDK" ]]; then
  echo "VitaSDK not found at: $VITASDK" >&2
  echo "Install it first, or export VITASDK=/path/to/vitasdk" >&2
  exit 1
fi

if ! command -v arm-vita-eabi-gcc >/dev/null 2>&1; then
  echo "arm-vita-eabi-gcc not found. Is \$VITASDK/bin on PATH?" >&2
  exit 1
fi

# --safe builds the restricted eboot. The default is UNSAFE (D42): uma0:
# capacity needs a devctl the safe authid refuses. The variant is part of the
# source identity: default and safe are different eboots and must not share a
# build number.
SAFE=OFF
VARIANT=""
if [[ "${1:-}" == "--safe" ]]; then
  SAFE=ON
  VARIANT="-safe"
fi

# A build number identifies a *distinct* build, not a compiler invocation.
#
# Rebuilding unchanged sources -- which happens constantly while checking for
# warnings -- reuses the previous number instead of burning a new one. The stamp
# lives beside BUILD_NUMBER, not in build/, so wiping the build directory cannot
# restart numbering into a collision with a VPK already on the console (D46).
STAMP="$ROOT/BUILD_NUMBER"
STATE="$ROOT/.source-stamp"

# Hash only what enters the VPK plus the build recipe. Test/preview edits do not
# burn a number; the safe flag does, because the authid and the eboot differ.
SOURCE_HASH=$(cd "$ROOT" && \
  find src sce_sys CMakeLists.txt build.sh -type f -print0 |
  sort -z | xargs -0 cat | shasum -a 256 | cut -d' ' -f1)
SOURCE_HASH="${SOURCE_HASH}:${SAFE}"

BUILD=$(cat "$STAMP" 2>/dev/null || echo 0)
case "$BUILD" in
  ''|*[!0-9]*) BUILD=0 ;;
esac

if [[ -f "$STATE" && "$(cat "$STATE")" == "$SOURCE_HASH" && "$BUILD" -gt 0 ]]; then
  echo "Sources unchanged since build $BUILD; rebuilding under the same number."
else
  BUILD=$((BUILD + 1))
  printf '%s\n' "$BUILD" > "$STAMP"
  printf '%s\n' "$SOURCE_HASH" > "$STATE"
fi

VERSION=$(sed -n 's/^set(VITA_VERSION[[:space:]]*"\([^"]*\)").*/\1/p' "$ROOT/CMakeLists.txt")

cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release \
  -DCOCKPIT_BUILD="$BUILD" -DCOCKPIT_SAFE="$SAFE"
cmake --build "$ROOT/build" --parallel

EXPECTED="$ROOT/build/cockpit-v${VERSION}-b${BUILD}${VARIANT}.vpk"

# Keep exactly one artifact. The VPK is the deliverable; `.vpk.out` and
# `_param.sfo` are packaging intermediates. Delete every other build's outputs,
# including the old pre-rename files, so "which build is on the Vita?" has one
# answer. The unstripped `build/cockpit` ELF is deliberately kept.
find "$ROOT/build" -maxdepth 1 -type f \
  \( -name 'cockpit-v*.vpk' -o -name 'cockpit-v*.vpk.out' \
     -o -name 'cockpit-v*.vpk_param.sfo' \) \
  ! -name "$(basename "$EXPECTED")" -delete

# Remove outputs from the retired system-plugin and on-demand clock builds.
rm -rf "$ROOT/build/system-plugin"
rm -f "$ROOT/build"/cockpit_clocks* "$ROOT/build"/libcockpit_clocks_stub*

if [[ ! -f "$EXPECTED" ]]; then
  echo "Build did not produce: $EXPECTED" >&2
  exit 1
fi

echo
echo "Built: $EXPECTED"
