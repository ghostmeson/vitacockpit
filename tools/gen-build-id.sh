#!/usr/bin/env bash
set -euo pipefail

# One build-identity source for the VPK, the app, the host tests, and the
# previews. CMake, build.sh, and both host scripts call this so ABOUT cannot
# disagree with the artifact name or the release tag.
#
#   tools/gen-build-id.sh <output-header> [ON|OFF|--safe] [build-number]
#
# The optional build number is the one CMake resolved for this artifact. A
# manual CMake invocation that passes -DCOCKPIT_BUILD=<n> then agrees with the
# header instead of silently reading a different BUILD_NUMBER.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT/build/generated/cockpit_build.h}"
SAFE_ARG="${2:-OFF}"
BUILD_ARG="${3:-}"

VERSION="$(sed -n 's/^set(VITA_VERSION[[:space:]]*"\([^"]*\)").*/\1/p' "$ROOT/CMakeLists.txt" | head -1)"
BUILD="$BUILD_ARG"
if [[ -z "$BUILD" ]]; then
  BUILD="$(tr -d '[:space:]' < "$ROOT/BUILD_NUMBER" 2>/dev/null || true)"
fi

if [[ -z "$VERSION" ]]; then
  echo "gen-build-id: VITA_VERSION not found in CMakeLists.txt" >&2
  exit 1
fi

if [[ -z "$BUILD" ]]; then
  echo "gen-build-id: BUILD_NUMBER is missing or empty" >&2
  exit 1
fi

# The package keeps the leading zero because APP_VER requires ##.##; the
# release label drops it so ABOUT reads 1.01 against the v1.01 tag.
RELEASE="$VERSION"
if [[ "$RELEASE" == 0* ]]; then
  RELEASE="${RELEASE:1}"
fi

SAFE=0
if [[ "$SAFE_ARG" == "ON" || "$SAFE_ARG" == "--safe" || "$SAFE_ARG" == "1" ]]; then
  SAFE=1
fi

mkdir -p "$(dirname "$OUT")"

cat > "$OUT" <<EOF
#ifndef COCKPIT_GENERATED_BUILD_H
#define COCKPIT_GENERATED_BUILD_H

#define COCKPIT_PACKAGE_VERSION "$VERSION"
#define COCKPIT_RELEASE_VERSION "$RELEASE"
#define COCKPIT_BUILD_NUMBER "$BUILD"
#define COCKPIT_SAFE $SAFE

#endif
EOF

echo "build identity: v$RELEASE b$BUILD$([[ $SAFE -eq 1 ]] && echo ' safe') -> $OUT"
