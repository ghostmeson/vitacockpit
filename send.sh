#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# build.sh leaves exactly one versioned VPK. There is no fixed fallback: a
# missing artifact must be an error, not a request to upload the wrong variant.
VPK="$(ls -t "$ROOT"/build/cockpit-v*.vpk 2>/dev/null | head -1 || true)"
if [[ -z "$VPK" ]]; then
  echo "No cockpit-v*.vpk in build/. Run ./build.sh first." >&2
  exit 1
fi

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <vita-ip>" >&2
  echo "Example: $0 192.168.1.42" >&2
  exit 1
fi

VITA_IP="$1"
NAME="$(basename "$VPK")"
VARIANT="default"
case "$NAME" in
  *-safe.vpk) VARIANT="safe" ;;
esac

echo "Uploading the $VARIANT build: $NAME"
echo "  to ftp://$VITA_IP:1337/ux0:/data/ ..."
curl -T "$VPK" "ftp://$VITA_IP:1337/ux0:/data/"

echo
echo "Uploaded. On the Vita:"
echo "  1. Open VitaShell"
echo "  2. Go to ux0:data"
echo "  3. Select $NAME and press Cross"
echo "  4. Launch the COCKPIT bubble"
